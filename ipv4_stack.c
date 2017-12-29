#include <rte_ip.h>

#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include "ipv4_stack.h"
#include "router.h"
#include "ethernet_stack.h"
#include "routing_table.h"
#include "global.h"

/*********************************
 *  Static function declarations *
 *********************************/
static int basic_chks(const void *pkt, uint16_t len);
static int lookup_and_fwd(intf_cfg_t *cfg, struct rte_mbuf *mbuf, 
                                const void *pkt);


/*********************************
 *  Inline function definitions  *
 *********************************/
static inline void drop_pkt(struct rte_mbuf *mbuf)
{
    rte_pktmbuf_free(mbuf);
}


/*********************************
 *      Function definitions     *
 *********************************/
/**
 * /brief Handle an received IPv4 packet.
 * 
 * Handle an IPv4 packet and forward it to the next hop if all sanity chacks
 * are okay. We drop the packet if it is addressed to this host because
 * we do not know what to do with it on a router.
 * 
 * \param cfg The configuration of the interface this packet was received on.
 * \param mbuf The DPDK buffer containing the complete frame. This parameter
 *              is neccessary for reusage in the case of forwarding and
 *              memory cleanup if we drop that packet.
 * \param pkt Pointer to the start of the IPv4 packet.
 * \param len Length of the IPv4 packet regarding the link layer.
 * 
 * \returns 0 if the packet was forwarded successfully or was addressed to this
 *              host.
 *              Errors: ERR_INV_PKT: The packet was invalid
 *                      ERR_TTL_EXP: TTL expired in transit. TTL < 0 after
 *                                      decrement.
 */
int handle_ipv4(intf_cfg_t *cfg, struct rte_mbuf *mbuf, 
                const void *pkt, uint16_t len)
{
    struct ipv4_hdr *hdr = (struct ipv4_hdr *)pkt;

    if(basic_chks(pkt, len) < 0) { // Drop the packet
        // If the check method does not provide an error message. Do it here
        #ifndef VERBOSE
        printf("Received invalid IPv4 packet. Dropping it!\n");
        #endif
        drop_pkt(mbuf);
        return ERR_INV_PKT;
    }

    // Check if we have to forward the packet or if it is addressed to this host
    if(hdr->dst_addr == cfg->ip_addr_be) { // Thanks, but i can not use it..
        #ifdef VERBOSE
        printf("Thanks for this nice IP packet, but i have to drop it!\n");
        #endif
        drop_pkt(mbuf);
        return 0;
    }

    // Is the TTL large enough to forward the packet?
    if(--hdr->time_to_live < 1) {
        #ifdef VERBOSE
        printf("Cannot forward the packet. TTL expired in transit.\n");
        #endif
        drop_pkt(mbuf);
        return ERR_TTL_EXP;
    }

    // Update the checksum
    hdr->hdr_checksum += rte_cpu_to_be_16(0x0100);

    return lookup_and_fwd(cfg, mbuf, pkt);
}

/*********************************
 *  Static function definitions  *
 *********************************/
/**
 * /brief Perform basic IPv4 packet validity checks.
 * 
 * This funciton performs basic (header) validity checks according to 
 * RFC 1812.
 * 
 * \param pkt Pointer to the header of the currently handled IPv4 packet.
 * \param len Length of the data reported by the link layer.
 * 
 * \return -1 if any error occured. 0 if the checks are okay.
 */
static int basic_chks(const void *pkt, uint16_t len)
{
    struct ipv4_hdr *hdr = (struct ipv4_hdr *)pkt;

    if(len < 20) { // IP packet is smaller than 20 bytes?
        #ifdef VERBOSE
        printf("IPv4 packet is smaller than 20 bytes. Dropping it!\n");
        #endif
        return -1;
    }

    uint16_t chksum = hdr->hdr_checksum;
    hdr->hdr_checksum = 0;
    if(rte_ipv4_cksum(hdr) != chksum) { // Invalid checksum
        #ifdef VERBOSE
        printf("IPv4 packet has an invalid checksum. Dropping it!\n");
        #endif
        return -1;
    }

    // Paste the original checksum back to the packet.
    // Calculating the new one is handled by the caller!
    hdr->hdr_checksum = chksum;
    if((hdr->version_ihl & 0xF0) != 0x40) { // Check if the version is 4
        #ifdef VERBOSE
        printf("IP stack cannot handle other IP versions than 4."
                    "Dropping the packet!\n");
        #endif
        return -1;
    }

    // IHL must be at least 20 byte -> 5 * 32 bit (4 byte)
    if((hdr->version_ihl & 0x0F) < 5) {
        #ifdef VERBOSE
        printf("IHL is less than 20. Dropping the packet!\n");
        #endif
        return -1;
    }

    // IHL increased to a 16 bit value
    uint16_t ihl_16 = ((uint16_t)((hdr->version_ihl & 0x000F))) << 2;
    if(rte_be_to_cpu_16(hdr->total_length) < ihl_16) {
        #ifdef VERBOSE
        printf("Total length is smaller than IHL. Dropping the packet!\n");
        #endif
        return -1;
    }

    // Additional test that is not conform to RFC 1812 but such a packet is
    // invalid!
    if(rte_be_to_cpu_16(hdr->total_length) != len) {
        #ifdef VERBOSE
        printf("Total length of IPv4 packet does not equal the packet length"
                    " reported by the link layer. Dropping it!\n");
        #endif
        return -1;
    }

    return 0;
}

/**
 * /brief Forward the packet to the right next hop.
 * 
 * This function traverses the routing table using the Longest-Prefix-Matching
 * (LPM) algorithm. If a suitable prefix is found, we send out the packet
 * to the specified egress interface to the next hop stored in the routing
 * table entry.
 * 
 * Currently this method uses the dummy_routing_table provided by the
 * instructors.
 * 
 * \param cfg Configuration of the ingress interface of the packet.
 * \param mbuf The rte_mbuf containing the packet. This is reused for sending.
 * \param pkt Pointer to the actual IPv4 payload. In the packet, the TTL must
 *              already be decreased!
 * 
 * \returns 0 on success.
 *          Errors: ERR_NO_ROUTE if we did not find a suitable route
 */
static int lookup_and_fwd(intf_cfg_t *cfg, struct rte_mbuf *mbuf, 
                            const void *pkt)
{
    uint32_t dst_addr = ((struct ipv4_hdr *)pkt)->dst_addr;

    struct routing_table_entry *entry = get_next_hop(dst_addr);

    if(entry == NULL) { // No entry found..
        #ifdef VERBOSE
        printf("Cannot get routing table entry for ip address: %d.%d.%d.%d\n",
                    (uint8_t)dst_addr, (uint8_t)(dst_addr >> 8),
                    (uint8_t)(dst_addr >> 16), (uint8_t)(dst_addr >> 24));
        #endif
        drop_pkt(mbuf);
        return ERR_NO_ROUTE;
    }

    return send_frame(cfg, mbuf, entry->dst_port, &entry->dst_mac);
}