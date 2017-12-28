#include <arpa/inet.h>

#include "routing_table.h"
#include "global.h"

/**********************************
 *    Global field definitions    *
 **********************************/
tmp_route_t *tmp_route_list = NULL;
tbl24_entry_t *tbl24 = NULL;
tbllong_entry_t *tbllong = NULL;
uint no_tbllong_entries = 0;


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

        while(*iterator != NULL && (*iterator)->netmask < netmask)
            *iterator = (*iterator)->nxt;

        if((new_line = malloc(sizeof(tmp_route_t))) == NULL)
            return ERR_MEM;

        // iterator is either NULL or there is a valid next entry
        // => The next pointer is always in a clean state
        new_line->nxt = *iterator;
        new_line->dst_net = dst_net;
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
    tmp_route_t *it_24 = tmp_route_list;
    uint32_t index = 0, hop_id = 0;
    uint8_t tmp = 0;
    tbl24_entry_t *tbl24_ent;

    uint ret = 0;

    if(tbl24 != NULL) {
        ret = ERR_MEM;
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

    // Check if we have a default route -> Yes: We do not have to fill the 
    // TBL24 with zeros
    if(it_24 != NULL && it_24->netmask != 0) { // No default route
        // We treat the entry [0|000000000000000] (valid entry and next hop ID 0) as
        // the invalid entry -> No route to host
        memset(tbl24, 0, TBL24_SIZE);
    }

    for(; it_24 != NULL; it_24 = it_24->nxt) {
        if(it_24->prf < 25) {
            for(
                index = it_24->dst_net >> 8;
                index < ((0xFFFFFFFF & ~it_24->netmask) + it_24->dst_net) >> 8;
                ++index
            ) {
                // We can simply override the entry here as this entry is more
                // specific than the ones before -> Sorting ;)
                tbl24[index].indicator = 0;
                tbl24[index].index = hop_id;
            }
        } else { // Prefix is longer than 24
            if(no_tbllong_entries >= TBLlong_MAX_ENTRIES) { // Enough space?
                printf("Not enough space in TBLlong!\n");
                ret = ERR_MEM;
                goto ERR;
            }

            // Get the corresponding TBL24 entry
            tbl24_ent = tbl24 + (it_24->dst_net >> 8);
            tbl24_ent->indicator = 1;

            // Either the valid is valid -> Take this entry and override the
            // ones this new route is a more specific one
            // If the entry is not valid, we set all new entries to invalid
            // (hop_id == 0), too
            memset(
                tbllong + no_tbllong_entries * 256,
                tbl24_ent->index,
                256 * sizeof(tbllong_entry_t)
            );

            tmp = (uint8_t) (it_24->dst_net & 0xFF);
            for(    
                index = no_tbllong_entries * 256 + tmp;
                index < no_tbllong_entries * 256
                        + (0xFF & (~it_24->netmask >> 8))
                        + tmp;
                ++index
            ) {
                // Override the entries we now know a more specific route
                tbllong[index].index = hop_id;
            }
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