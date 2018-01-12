/*
 * This file contains additional functions and structures for the routing table.
 * The reason why we use an additional file is that it is easier to leave the
 * 'routing_table.h' exactly as it is defined in the framework.
 * Therefore, we do not have to care if we might have changed the interface.
 * 
 * YES I know: This is a little bit ugly...
 */

#ifndef ROUTING_TABLE_ADDITIONAL_H__
#define ROUTING_TABLE_ADDITIONAL_H__

#include <stdbool.h>

#include <rte_config.h>
#include <rte_ether.h>

#include "routing_table.h"

#define TBL24_SIZE ((2 << 23) * sizeof(tbl24_entry_t))
#define TBLlong_MAX_ENTRIES 4096
// 4096 entries are possible -> If gcc uses the 8 bit as we said -> 1MB
// In the paper about Dir-24-8 this value was recommended -> Use it
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
    uint32_t dst_net_cpu_bo; // network and netmask in cpu endianness
    uint32_t netmask_cpu_bo;
    uint8_t prf;
	uint8_t intf;
    // As we can only store 8 bits in the TBLlong anyway, we can use 8 bits here
    uint8_t hop_id;
	struct ether_addr dst_mac; // next hop MAC
    struct tmp_route *nxt;
} tmp_route_t;

typedef struct tbl24_entry {
    uint16_t indicator:1; // Valid entry or lookup in TBLlong
    uint16_t index:15; 
}__attribute__((packed)) tbl24_entry_t;

typedef struct tbllong_entry {
    uint8_t index;
}__attribute__((packed)) tbllong_entry_t;

typedef struct routing_table_entry rt_entry_t;


/**********************************
 *     Function declarations      *
 **********************************/
void clean_tmp_routing_table(void);
void clean_routing_table(void);

/**********************************
 *   Global field declarations    *
 **********************************/
extern tmp_route_t *tmp_route_list;
extern tbl24_entry_t *tbl24;
extern tbllong_entry_t *tbllong;
extern uint no_tbllong_entries;
extern rt_entry_t *nxt_hops_map;
extern uint no_nxt_hops;
#endif

