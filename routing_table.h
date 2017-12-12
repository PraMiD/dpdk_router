#ifndef ROUTING_TABLE_H__
#define ROUTING_TABLE_H__

#include <stdbool.h>

#include <rte_config.h>
#include <rte_ether.h>

/**********************************
 *     Structure definitions      *
 **********************************/
typedef struct routing_table_entry {
    uint32_t dst_net;
    uint32_t netmask;
	uint8_t intf;
	struct ether_addr dst_mac; // next hop MAC
    struct routing_table_entry *nxt;
} routing_table_entry_t;


/**********************************
 *     Function declarations      *
 **********************************/
int install_route(uint32_t dst_net, uint8_t prf,
                    uint8_t intf, struct ether_addr* mac);
void print_routes();
void print_port_id_to_mac();
void build_routing_table();
void print_next_hop_tab();
void print_routing_table_entry(struct routing_table_entry* info);
struct routing_table_entry* get_next_hop(uint32_t ip);

/**********************************
 *   Global field declarations    *
 **********************************/
extern routing_table_entry_t *routing_table;

#endif

