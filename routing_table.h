#ifndef ROUTING_TABLE_H__
#define ROUTING_TABLE_H__

#include <stdbool.h>

#include <rte_config.h>
#include <rte_ether.h>

#define TBL24_SIZE ((2 << 23) * sizeof(tbl24_entry_t))
#define TBLlong_MAX_ENTRIES 4096
// 4096 entries are possible -> If gcc uses the 8 bit as we said -> 1MB
#define TBLlong_SIZE (4096 * sizeof(tbllong_entry_t) * 256)
#define INIT_NO_NXT_HOPS 20

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
    uint8_t prf;
	uint8_t intf;
    // As we can only store 8 bits in the TBLlong anyway, we can use 8 bits here
    uint8_t hop_id;
	struct ether_addr dst_mac; // next hop MAC
    struct tmp_route *nxt;
} tmp_route_t;

typedef struct routing_table_entry {
    uint8_t dst_port;
    struct ether_addr dst_mac;
} rt_entry_t;

typedef struct tbl24_entry {
    uint16_t indicator:1; // Valid entry or lookup in TBLlong
    uint16_t index:15; 
}__attribute__((packed)) tbl24_entry_t;

typedef struct tbllong_entry {
    uint8_t index;
}__attribute__((packed)) tbllong_entry_t;


/**********************************
 *     Function declarations      *
 **********************************/
int add_route(uint32_t dst_net, uint8_t prf,
                    struct ether_addr* mac, uint8_t intf);
void print_routes(void);
void print_port_id_to_mac(void);
int build_routing_table(void);
void print_next_hop_tab(void);
void print_routing_table_entry(struct routing_table_entry* info);
void clean_tmp_routing_table(void);
struct routing_table_entry* get_next_hop(uint32_t ip);

/**********************************
 *   Global field declarations    *
 **********************************/
extern tmp_route_t *tmp_route_list;
extern tbl24_entry_t *tbl24;
extern tbllong_entry_t *tbllong;
extern uint no_tbllong_entries;
extern rt_entry_t *nxt_hops;
extern uint no_nxt_hops;
#endif

