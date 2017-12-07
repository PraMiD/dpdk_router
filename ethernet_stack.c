#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>

#include "ethernet_stack.h"
#include "router.h"
#include "arp_stack.h"
#include "global.h"

/**
 * \brief Handle an ethernet frame.
 * 
 * This method is the starting point of packet handling.
 * It checks the ethernet frame and distributes the frame/packet to the
 * responsible L3 stack or ARP stack.
 * 
 * If an unknown L3 protocol is used we return ERR_NOT_IMPL.
 * 
 * \param cfg The interface configuration of the interface this thread is 
 *              responsible for.
 * \param mbuf Pointer to the buffer that contains the currently handled packet.
 * 
 * \return 0 if the frame was handled successfully.
 *          Errors: ERR_NOT_IMPL: No stack for the given ethertype.
 *                  ERR_INV_PKT: If the received frame was currupted or the 
 *                               ARP/L3 stack was not able to handle
 *                               the packet or the packet is too short
 */
int handle_frame(intf_cfg_t *cfg, struct rte_mbuf *mbuf)
{
    struct ether_hdr *hdr = NULL;

    if(rte_pktmbuf_data_len(mbuf) < ETHER_HDR_LEN)
        return ERR_INV_PKT;

    hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);

    // Check if this packet was sent to my interface or broadcast
    if(!is_broadcast_ether_addr(&hdr->d_addr)
        && !is_same_ether_addr(&hdr->d_addr, &cfg->ether_addr)) {
        return 0;
    }

    switch(rte_be_to_cpu_16(hdr->ether_type)) {
        case ETHER_TYPE_IPv4:
            return 0;
        case ETHER_TYPE_ARP:
            // We do not check if an ARP packet is addressed to MAC_BROADCASE
            // This is an efficiency problem of the sender not our router!
            handle_arp(cfg,
                        mbuf,
                        ((char *)hdr) + ETHER_HDR_LEN,
                        rte_pktmbuf_data_len(mbuf) - ETHER_HDR_LEN
                    ); // Not aware of VLANs!
        default:
            return ERR_NOT_IMPL; 
    }
}

/**
 * /brief Send out a frame
 * 
 * This function send a frame and sets the destination and source IP address
 * according to the given information.
 * 
 * \param cfg The configuration of the interface the packet
 *              was received on/this core
 * \param mbuf The buffer where the packet was received in. We require this 
 *              for buffer reusage.
 * \param intf The interface we shall send the packet out on.
 * \param d_ether The destination ethernet address.
 * 
 * \return 0 on success. Currently the only possible value.
 */
int send_frame(intf_cfg_t *cfg, struct rte_mbuf *mbuf,
                uint8_t intf, struct ether_addr *d_ether) {
    struct ether_hdr *hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);

    ether_addr_copy(d_ether, &hdr->d_addr);
    rte_eth_macaddr_get(intf, &hdr->s_addr);

    while (!rte_eth_tx_burst(intf, cfg->lcore - 1, &mbuf, 1));

    return 0;
}