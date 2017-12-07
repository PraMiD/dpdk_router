#include <stdio.h>

#include <rte_arp.h>

#include "arp_stack.h"
#include "router.h"
#include "global.h"
#include "ipv4_stack.h"
#include "ethernet_stack.h"


/**********************************
 *  Static function declarations  *
 **********************************/
static int chk_valid_handle(char *hdr, int len, intf_cfg_t *cfg);

/**********************************
 *      Function definitions      *
 **********************************/
/**
 * \brief Handle an incoming ARP packet.
 * 
 * This function handles incoming ARP packets.
 * Currently on ARP Requests from IPv4 to ethernet are possible.
 * We do packet sanitization and answer the request if everything is okay.
 * 
 * \param cfg The configuration of the interface this packet was received on.
 * \param mbuf The receiver buffer. We need this parameter to reuse the buffer
 *              on sending the reply.
 * \param pkt The ARP packet contained in the buffer.
 * \param len The length of the ARP packet.
 * 
 * \return 0 If reply was sent.
 *          Errors: ERR_INV_PKT: Invalid ARP packet received.
 *                  ERR_NOT_IMPL: We do not support this type of requests.
 *                  ERR_NOTFORME: Wrong destination IP address.
 */
int handle_arp(intf_cfg_t *cfg, struct rte_mbuf *mbuf, char *pkt, int len)
{
    struct arp_hdr *hdr = (struct arp_hdr *)pkt;
    int err = 0;

    
    if((err = chk_valid_handle(pkt, len, cfg)) < 0)
        return err;

    // Handle the packet (We know that this is an ARP request from IPv4 to ETH)
    // hdr is valid and we can use it after this check

    ether_addr_copy(&hdr->arp_data.arp_sha, &hdr->arp_data.arp_tha);
    ether_addr_copy(&cfg->ether_addr, &hdr->arp_data.arp_sha);
    hdr->arp_data.arp_tip = hdr->arp_data.arp_sip;
    hdr->arp_data.arp_sip = cfg->ip_addr_be;
    hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);

    printf("TIP: 0x%x\n", hdr->arp_data.arp_tip);

    #ifdef VERBOSE
    printf("Sent ARP reply on interface: %d\n", cfg->intf);
    #endif

    // We handled our part -> Ethernet stack has to handle the source and
    // destination MAC
    return send_frame(cfg, mbuf, cfg->intf, &hdr->arp_data.arp_tha);
}

/**********************************
 *   Static function definitions  *
 **********************************/
/**
 * /brief Check if we can handle this ARP packet and if it is valid.
 * 
 * The function tests if this ARP packet is a request for a ethernet address
 * for a given IPv4 address and if the data is long enough to contain an
 * ARP packet.
 * In addition, we test if the HW address and protocol address lengths are
 * valid for those type of addresses.
 * 
 * \param hdr The header of the ARP packet.
 * \param len The length of the data we have received.
 * \param cfg The configuration of the interface this packet was received on.
 * 
 * \return 0 if everything is okay.
 *          Errors: ERR_INV_PKT: Not from IPv4 to ether, invalid
 *                                  address lengths or packet to short/long.
 *                  ERR_NOT_IMPL: ARP operation is not a request.
 *                  ERR_NOTFORME: IP address does not match the one on
 *                                  this interface.
 */
static int chk_valid_handle(char *pkt, int len, intf_cfg_t *cfg) {
    struct arp_hdr *hdr = (struct arp_hdr *)pkt;

    if(len != ARP_PKT_LEN) {
        #ifdef VERBOSE
        printf("ARP packet with an invalid length: 0x%x\n", len);
        #endif

        return ERR_INV_PKT;
    }

    if(!(rte_be_to_cpu_16(hdr->arp_op) == ARP_OP_REQUEST)) {
        // Can not handle such packets
        #ifdef VERBOSE
        printf("Not able to handle this ARP packet. Operation: 0x%x\n", 
                    rte_be_to_cpu_16(hdr->arp_op)
                );
        #endif
        return ERR_NOT_IMPL;
    }

    if(hdr->arp_data.arp_tip != cfg->ip_addr_be) {
        #ifdef VERBOSE
        printf("ARP packet was not sent to this hosts IP: TIP: 0x%x\n", 
                    hdr->arp_data.arp_tip
                );
        #endif
        return ERR_NOTFORME;
    }

    if(!(rte_be_to_cpu_16(hdr->arp_hrd) == ARP_HRD_ETHER)) {
        // Can not resolve to this type of address
        #ifdef VERBOSE
        printf("Not able to handle this ARP packet."
                    "Unknown HW address type: 0x%x\n", 
                    rte_be_to_cpu_16(hdr->arp_hrd)
                );
        #endif
        return ERR_INV_PKT;
    }

    if(!(rte_be_to_cpu_16(hdr->arp_pro) == ETHER_TYPE_IPv4)) {
        // Can not resolve this type of address
        #ifdef VERBOSE
        printf("Not able to handle this ARP packet."
                    "Unknown protocol address type: 0x%x\n", 
                    rte_be_to_cpu_16(hdr->arp_pro)
                );
        #endif
        return ERR_INV_PKT;
    }

    // Okay we are able to resolve this kind of address
    // Quick packet sanitizing
    if(!(hdr->arp_hln == ETHER_ADDR_LEN) || !(hdr->arp_pln == IPv4_ADDR_LEN)) {
        #ifdef VERBOSE
        printf("Not able to handle this ARP packet."
                    "Invalid protocol or HW address length\n"
                );
        #endif
        return ERR_INV_PKT;
    }

    return 0;
}