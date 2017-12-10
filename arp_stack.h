/**
 * This file contains methods to handle ARP packets.
 */
#ifndef ARP_STACK_H__
#define ARP_STACK_H__

#include "router.h"

#define ARP_PKT_LEN 28

extern int handle_arp(intf_cfg_t *cfg, struct rte_mbuf *mbuf,
                         char *pkt, uint16_t len);

#endif