#ifndef ROUTER_H__
#define ROUTER_H__
#include <rte_ether.h>

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

// Size of the receive buffer of a single thread
#define THREAD_BUFSIZE 64


/**********************************
 *  Static structure definitions  *
 **********************************/
typedef struct intf_cfg {
    uint8_t intf;
    uint32_t ip_addr_be; // Efficiency reason: IP address in big endian format
    struct ether_addr ether_addr;
    // The lcore argument tells the router_thread the lcore it is executed in
    // In addition, this number-1 gives us the transeive queue number we have
    // to use for ever interface
    uint16_t lcore;
    uint16_t num_rx_queues;
    struct intf_cfg *nxt;
} intf_cfg_t;

/**********************************
 *         Public fields          *
 **********************************/
extern intf_cfg_t *intf_cfgs;


/**********************************
 *     Function declarations      *
 **********************************/
int parse_args(int argc, char **argv);
int start_router();
int add_intf_cfg(uint8_t intf, uint32_t ip_addr);

#endif