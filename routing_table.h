#ifndef ROUTING_TABLE_H__
#define ROUTING_TABLE_H__

#include <stdbool.h>

#include <rte_config.h>
#include <rte_ether.h>

/**********************************
 *     Structure definitions      *
 **********************************/

/*
 * We use this struct to store all routing table entries read from the command
 * line sorted.
 * This sorted list simplifies the building procedure.
 */
typedef struct tmp_route {
    uint32_t dst_net;
    uint32_t netmask;
	uint8_t intf;
	struct ether_addr dst_mac; // next hop MAC
    struct tmp_route *nxt;
} tmp_route_t;

struct routing_table_entry {
    uint8_t dst_port;
    struct ether_addr dst_mac;
};


/**********************************
 *     Function declarations      *
 **********************************/
int install_route(uint32_t dst_net, uint8_t prf,
                    uint8_t intf, struct ether_addr* mac);
void clean_tmp_routing_table(void);
void print_routes(void);
void print_port_id_to_mac(void);
void build_routing_table(void);
void print_next_hop_tab(void);
void print_routing_table_entry(struct routing_table_entry* info);
struct routing_table_entry* get_next_hop(uint32_t ip);

/**********************************
 *   Global field declarations    *
 **********************************/
extern tmp_route_t *tmp_route_list;

#endif

