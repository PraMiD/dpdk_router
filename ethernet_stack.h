/**
 * This file defines methods tho handle ethernet frames.
 */
#ifndef ETHERNET_STACK_H__
#define ETHERNET_STACK_H__

#include <rte_mbuf.h>

#include "router.h"

int handle_frame(intf_cfg_t *cfg, struct rte_mbuf *mbuf);
int send_frame(intf_cfg_t *cfg, struct rte_mbuf *mbuf,
                uint8_t intf, struct ether_addr *d_ether);

#endif