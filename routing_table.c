#include <arpa/inet.h>

#include "routing_table.h"
#include "global.h"

/**********************************
 *    Global field definitions    *
 **********************************/
tmp_route_t *tmp_route_list = NULL;


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
 * /param mac The MAC of the next hop.
 * /param intf The interface where we can reach the next hop.
 * 
 * /return 0 on success.
 *      Errors: ERR_MEM, ERR_GEN
 */
int install_route(uint32_t dst_net, uint8_t prf,
                    uint8_t intf, struct ether_addr* mac) {
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
