#include <arpa/inet.h>

#include <rte_ether.h>

#include "routing_table.h"
#include "global.h"

/**********************************
 *    Global field definitions    *
 **********************************/
tmp_route_t *tmp_route_list = NULL;
tbl24_entry_t *tbl24 = NULL;
tbllong_entry_t *tbllong = NULL;
uint no_tbllong_entries = 0;
rt_entry_t *nxt_hops_map = NULL;
uint no_nxt_hops = 0;


/**********************************
 *  Static funciton decalarations *
 **********************************/
static int alloc_hop_ids(void);

/**********************************
 *      Function definitions      *
 **********************************/
/**
 * /brief Add a new route to the temporary list of routes.
 * 
 * This method adds a new route to the temporary list of routes.
 * Sorting is handled by this method to simplify the construction of the
 * Dir-24-8 routing tables.
 * 
 * The entries are sorted from shortest to longest prefixes.
 * 
 * /param dst_net The IP address of the destination in big endian format.
 * /param prf The CIDR prefix of the destination.
 * /param intf The interface where we can reach the next hop.
 * /param mac The MAC of the next hop.
 * 
 * /return 0 on success.
 *      Errors: ERR_MEM, ERR_GEN
 */
int add_route(uint32_t dst_net, uint8_t prf,
                    struct ether_addr* mac, uint8_t intf) {
        tmp_route_t **iterator = &tmp_route_list;
        tmp_route_t *new_line = NULL;

        // Strip away a possible host part from the dst network.
        uint32_t netmask = 0;
        // The expression below cannot handle prefixes of 0 nicely
        // (without casting to 64 bits and then back to 32)
        if(prf != 0) {
            netmask = htonl(~((1 << (32 - prf)) - 1));
        }
        dst_net &= netmask;

        while(*iterator != NULL && (*iterator)->netmask > netmask)
            iterator = &(*iterator)->nxt;

        if((new_line = malloc(sizeof(tmp_route_t))) == NULL)
            return ERR_MEM;

        // iterator is either NULL or there is a valid next entry
        // => The next pointer is always in a clean state
        new_line->nxt = *iterator;
        new_line->dst_net =  dst_net;
        new_line->netmask = netmask;
        new_line->prf = prf;
        new_line->intf = intf;
        memcpy(&new_line->dst_mac, mac, sizeof(struct ether_addr));

        #ifdef VERBOSE
        printf("Added route for destination network %d.%d.%d.%d"
                    " with netmask %d.%d.%d.%d to temporary routing table.\n",
                    // dst_net is big endian
                    (uint8_t)dst_net, (uint8_t)(dst_net >> 8),
                    (uint8_t)(dst_net >> 16), (uint8_t)(dst_net >> 24),

                    // netmask is in big endian
                    (uint8_t)netmask, (uint8_t)(netmask >> 8),
                    (uint8_t)(netmask >> 16), (uint8_t)(netmask >> 24)
        );
        #endif

        *iterator = new_line;

        return 0;
}

/*
 * \brief Clear all temporary routing table entries.
 * 
 * The method is used to clear all temporary routing table entries.
 */
void clean_tmp_routing_table(void)
{
    tmp_route_t *it = tmp_route_list, *nxt;

    while(it != NULL) {
        nxt = it->nxt;
        free(it);
        it = nxt;
    }

    tmp_route_list = NULL;
}


int build_routing_table(void)
{
    tmp_route_t *route_it = tmp_route_list;
    uint32_t index = 0, dst_net_cpu_bo = 0, netmask_cpu_bo = 0;
    uint8_t tmp = 0;
    tbl24_entry_t *tbl24_ent;

    uint ret = 0;

    uint test = 0;

    if(tbl24 != NULL) {
        ret = ERR_GEN;
        goto ERR;
    }

    // Allocate memory for TBL24
    if((tbl24 = malloc(TBL24_SIZE)) == NULL) {
        printf("Cannot allocate memory for TBL24!\n");
        ret = ERR_GEN;
        goto ERR;
    }

    // Allocate memory for TBLlong
    if((tbllong = malloc(TBLlong_SIZE)) == NULL) {
        printf("Cannot allocate memory for TBLlong!\n");
        ret = ERR_MEM;
        goto ERR;
    }

    // build next hops table
    if((ret = alloc_hop_ids()) != 0) {
        printf("Cannot build next hops table!\n");
        goto ERR;
    }

    // Check if we have a default route -> Yes: We do not have to fill the 
    // TBL24 with zeros
    if(route_it != NULL && route_it->netmask != 0) { // No default route
        // We treat the entry [0|000000000000000] (TBL24 valid and
        // next hop ID 0) as the 'no route to host' entry
        memset(tbl24, 0, TBL24_SIZE);
    }

    for(; route_it != NULL; route_it = route_it->nxt) {
        dst_net_cpu_bo = rte_be_to_cpu_32(route_it->dst_net);
        netmask_cpu_bo = rte_be_to_cpu_32(route_it->netmask);
        if(route_it->prf < 25) {
            for(
                index = dst_net_cpu_bo >> 8;
                index <= 
                    ((0xFFFFFFFF & ~netmask_cpu_bo) + dst_net_cpu_bo) >> 8;
                ++index
            ) {
                // We can simply override the entry here as this entry is more
                // specific than the ones before -> Sorting ;)
                tbl24[index].indicator = 0;
                tbl24[index].index = route_it->hop_id;
            }
        } else { // Prefix is longer than 24
            if(no_tbllong_entries >= TBLlong_MAX_ENTRIES) { // Enough space?
                printf("Not enough space in TBLlong!\n");
                ret = ERR_MEM;
                goto ERR;
            }

            // Get the corresponding TBL24 entry
            tbl24_ent = tbl24 + (dst_net_cpu_bo >> 8);
            tbl24_ent->indicator = 1;
            tbl24_ent->index = no_tbllong_entries * 256;

            // Either the valid is valid -> Take this entry and override the
            // ones this new route is a more specific one
            // If the entry is not valid, we set all new entries to invalid
            // (hop_id == 0), too
            memset(
                tbllong + tbl24_ent->index,
                tbl24_ent->index,
                256 * sizeof(tbllong_entry_t)
            );

            tmp = (uint8_t)dst_net_cpu_bo;
            for(
                index = tbl24_ent->index + tmp;
                index <= (uint8_t)(
                        tbl24_ent->index
                        + ((uint8_t)(0xFF & ~netmask_cpu_bo))
                        + tmp);
                ++index
            ) {
                // Override the entries we now know a more specific route
                tbllong[index].index = route_it->hop_id;
            }

            no_tbllong_entries++;
        }
    }

    // We do not need those entries now -> Delete them and free the used memory
    clean_tmp_routing_table();

    RET:
    return ret;

    ERR:
    if(tbl24 != NULL) {
        free(tbl24);
        tbl24 = NULL;
    }

    if(tbllong != NULL) {
        free(tbllong);
        tbllong = NULL;
    }

    goto RET;
}


/*
 * /brief Allocate the next hops specified in all routes to next hop IDs used
 *          in the DIR-24-8 structure.
 * 
 * This function iterates over all route definitions and assigns every next hop
 * specified in the route a next hop ID. The mapping from next hop ID
 * to the outgoing interface and destination MAC (rt_entry_t) is stored in the 
 * nxt_hops_map array.
 * 
 * If two routes have the same egress interface and the same destination MAC
 * specified, we will assign them the sme next hop ID.
 * 
 * \return 0 on success.
 *          Errors: ERR_MEM: Could not (re-)allocate memory for the
 *                              nxt_hops_map.
 *                  ERR_GEN: If there are more than 255 next hops.
 */
static int alloc_hop_ids(void)
{
    tmp_route_t *it = tmp_route_list, *prf_it = tmp_route_list;
    uint nxt_hop_id = 1; // 0 is used as special 'no next hop' value
    rt_entry_t *tmp_ptr = NULL;

    // Allocate memory for the nxt_hops_map array
    no_nxt_hops = INIT_NO_NXT_HOPS;
    if((nxt_hops_map = malloc(no_nxt_hops * sizeof(rt_entry_t))) == NULL) {
        printf("Not enough memory for the next hops table!\n");
        return ERR_MEM;
    }
    memset(nxt_hops_map, 0, INIT_NO_NXT_HOPS * sizeof(rt_entry_t));

    for(; it != NULL; it = it->nxt) {
        for(
            prf_it = tmp_route_list;
            prf_it != NULL && it != prf_it;
            prf_it = prf_it->nxt
        ) {
            if(
                it->intf == prf_it->intf
                && is_same_ether_addr(&it->dst_mac, &prf_it->dst_mac)
            ) {
                it->hop_id = prf_it->hop_id;
                break;
            }
        }

        // No next hop matched the one of the current route
        if(it == prf_it) {
            if(nxt_hop_id >= no_nxt_hops) {
                no_nxt_hops += INIT_NO_NXT_HOPS;
                if(no_nxt_hops > 255) {
                    printf("To many next hops (>255) cannot be handled by "
                    "DIR-24-8-BASIC. Aborting...\n");
                    return ERR_GEN;
                }
                if(
                    (tmp_ptr = 
                        realloc(nxt_hops_map, INIT_NO_NXT_HOPS * sizeof(rt_entry_t))
                    ) == NULL
                ) {
                    printf("Cannot increase the size of the next hops table!\n");
                    free(nxt_hops_map);
                    return ERR_MEM;
                }
            }
            
            nxt_hops_map[nxt_hop_id].dst_port = it->intf;
            ether_addr_copy(&it->dst_mac, &nxt_hops_map[nxt_hop_id].dst_mac);
            it->hop_id = nxt_hop_id++;
        }
    }

    return 0;
}