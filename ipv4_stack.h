/**
 * This file contains definitions to handle IPv4 packets.
 */
#ifndef IPV4_STACK_H__
#define IPV4_STACK_H__

#include <rte_mbuf.h>

#include "router.h"

#define IPv4_ADDR_LEN 0x04

extern int handle_ipv4(intf_cfg_t *cfg, struct rte_mbuf *mbuf,
                        const void *pkt, uint16_t len);

#endif