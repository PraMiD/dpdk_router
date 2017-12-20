#ifndef ROUTING_TABLE_H__
#define ROUTING_TABLE_H__

#include <stdbool.h>

#include <rte_config.h>
#include <rte_ether.h>

/**********************************
 *     Structure definitions      *
 **********************************/

// This is the routing table entry I used in my implementation
// This structure is currently not used, but I missed in the beginning
// of the exercise that we shall use the dummy routing table.
// In addition, I will reuse this structure while building the DIR24-8
// routing table. Therefore, I leave it here.
typedef struct rt_entry {
    uint32_t dst_net;
    uint32_t netmask;
	uint8_t intf;
	struct ether_addr dst_mac; // next hop MAC
    struct rt_entry *nxt;
} routing_table_entry_t;

struct routing_table_entry {
    uint8_t dst_port;
    struct ether_addr dst_mac;
};


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

