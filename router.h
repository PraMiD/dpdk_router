#ifndef ROUTER_H__
#define ROUTER_H__

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>


/**********************************
 *  Static structure definitions  *
 **********************************/
typedef struct intf_config {
    uint8_t intf;
    uint32_t ip_addr;
    struct intf_config *nxt;
} intf_config_t;


/**********************************
 *     Function declarations      *
 **********************************/
int add_intf_config(uint8_t intf, uint32_t ip_addr);

int router_thread(void* arg);
void parse_route(char *route);
int parse_args(int argc, char **argv);
void start_thread(uint8_t port);

#endif

