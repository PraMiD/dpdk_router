#include <rte_ip.h>

#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include "ipv4_stack.h"
#include "router.h"
#include "ethernet_stack.h"
#include "global.h"

/*********************************
 *  Static function declarations *
 *********************************/
static int basic_chks(const void *pkt, uint16_t len);
static int lookup_and_send(intf_cfg_t *cfg, struct rte_mbuf *mbuf, 
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
        printf("Received invalid IPv4 packet. Dropping it!");
        #endif
        drop_pkt(mbuf);
        return ERR_INV_PKT;
    }

    // Check if we have to forward the packet or if it is addressed to this host
    if(hdr->dst_addr == cfg->ip_addr_be) { // Thanks, but i can not use it..
        #ifdef VERBOSE
        printf("Thanks for this nice IP packet, but i have to drop it!");
        #endif
        drop_pkt(mbuf);
        return 0;
    }

    // Is the TTL large enough to forward the packet?
    if(--hdr->time_to_live < 1) {
        #ifdef VERBOSE
        printf("Cannot forward the packet. TTL expired in transit.");
        #endif
        drop_pkt(mbuf);
        return ERR_TTL_EXP;
    }

    // Update the checksum
    // We could have a faster implementation if we assume that the machine
    // has little endian format. However, this approach is more portable!
    hdr->hdr_checksum = htonl(ntohl(hdr->hdr_checksum) - 1);

    return lookup_and_send(cfg, mbuf, pkt, len);
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
        printf("IPv4 packet is smaller than 20 bytes. Dropping it!");
        #endif
        return -1;
    }

    uint16_t chksum = hdr->hdr_checksum;
    hdr->hdr_checksum = 0;
    if(rte_ipv4_cksum(hdr) != chksum) { // Invalid checksum
        #ifdef VERBOSE
        printf("IPv4 packet has an invalid checksum. Dropping it!");
        #endif
        return -1;
    }
    // Paste the original checksum back to the packet.
    //Calculating the new one is handled by the caller!
    hdr->hdr_checksum = chksum;

    if((hdr->version_ihl & 0xF0) != 0x4) { // Check if the version is 4
        #ifdef VERBOSE
        printf("IP stack cannot handle other IP versions than 4."
                    "Dropping the packet!");
        #endif
        return -1;
    }

    if((hdr->version_ihl & 0x0F) < 20) { // IHL must be at least 20
        #ifdef VERBOSE
        printf("IHL is less than 20. Dropping the packet!");
        #endif
        return -1;
    }

    // IHL increased to a 16 bit value
    uint16_t ihl_16 = (uint16_t)((hdr->version_ihl & 0x000F));
    if(hdr->total_length < ihl_16) {
        #ifdef VERBOSE
        printf("Total length is smaller than IHL. Dropping the packet!");
        #endif
        return -1;
    }

    // Additional test that is not conform to RFC 1812 but such a packet is
    // invalid!
    if(hdr->total_length != len) {
        #ifdef VERBOSE
        printf("Total length of IPv4 packet does not equal the packet length"
                    " reported by the link layer. Dropping it!");
        #endif
        return -1;
    }

    return 0;
}

static int lookup_and_fwd(intf_cfg_t *cfg, struct rte_mbuf *buf, 
                            const void *pkt)
{

}