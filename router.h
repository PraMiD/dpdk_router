#ifndef ROUTER_H__
#define ROUTER_H__

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>


/**********************************
 *  Static structure definitions  *
 **********************************/
typedef struct intf_cfg {
    uint8_t intf;
    uint32_t ip_addr;
    uint16_t lcore;
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

