#include "routing_table.h"
#include "global.h"

/**********************************
 *    Global field definitions    *
 **********************************/
routing_table_entry_t *routing_table = NULL;


/**********************************
 *      Function definitions      *
 **********************************/
/**
 * /brief Add a new route to the routing/forwarding table.
 * 
 * This methods adds a new route to the routing table. It handles the sorting
 * to enable the LPM algorithm. We will copy the mac address to a new struct.
 * 
 * /param dst_net The IP address of the destination.
 * /param prf The CIDR prefix of the destination.
 * /param mac The MAC of the next hop.
 * /param intf The interface where we can reach the next hop.
 * 
 * /return 0 on success.
 *      Errors: ERR_MEM, ERR_GEN
 */
int install_route(uint32_t dst_net, uint8_t prf,
                    uint8_t intf, struct ether_addr* mac) {
        routing_table_entry_t **iterator = &routing_table;
        routing_table_entry_t *new_line = NULL;

        // Store the strip away a possible host part from the dst network.
        uint32_t netmask = (1 << (32 - prf)) - 1;
        dst_net &= netmask;

        while(*iterator != NULL && (*iterator)->netmask > netmask)
            *iterator = (*iterator)->nxt;

        if((new_line = malloc(sizeof(routing_table_entry_t))) == NULL)
            return ERR_MEM;

        new_line->nxt = *iterator;
        new_line->dst_net = dst_net;
        new_line->netmask = netmask;
        new_line->intf = intf;
        memcpy(&new_line->dst_mac, mac, sizeof(struct ether_addr));

        *iterator = new_line;

        return 0;
}
