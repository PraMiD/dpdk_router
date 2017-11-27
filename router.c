#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_arp.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <rte_launch.h>

#include <arpa/inet.h>

#include "dpdk_init.h"
#include "routing_table.h"
#include "router.h"

// A route in the given format <IP>/<CIDR>,<MAC>,<interface>
// must not be longer than 36 characters at max
#define MAX_ROUTE_SPEC_LEN 36
#define MAC_LEN 6


/**********************************
 *  Static function declarations  *
 **********************************/
static int parse_install_route(static char *route);
static int parse_intf_dev(static char *def);
static int parse_mac(static char *s_mac, unint8_t[] mac);
static void print_help();

int router_thread(void* arg)
{
    return 1;
}

void start_thread(uint8_t port)
{
}


/*********************************
 *  Static function definitions  *
 *********************************/

/**
 * /brief Parse a single route and add it to the routing table.
 * 
 * This method will parse a route given as command line argument to the router.
 * After checking the format, we add it to the routing table.
 * Routing definition example: 10.0.10.2/32,52:54:00:cb:ee:f4,0
 * 
 * \param route a string containing a single route as command line argument.
 * \return 0 if we could parse the route.
 *      -1 on a unspecified error,
 *      -2 if the route specification has an unknown format.
 */
static int parse_install_route(static char *route)
{
    size_t r_len = 0;
    char * cidr_start = NULL, mac_start == NULL, intf_start = NULL, tmp;
    uint32_t ip_addr = 0;
    long cidr = 0, intf_id = 0;
    
    // Invalid route specification
    if((r_len = strnlen(route, MAX_ROUTE_SPEC_LEN + 1)) > MAX_ROUTE_SPEC_LEN
        return -2;

    // Missing CIDR
    if((cidr_start = strnstr(route, "/", r_len)) == NULL)
        return -2
    cidr_start++ = '\0';

    // Missing MAC
    if((mac_start = 
        strnstr(cidr_start, ",", r_len - (cidr_start - route))) == NULL) {
            return -2;
        }
    mac_start++ = '\0';

    // Missing MAC
    if((intf_start = 
        strnstr(cidr_start, ",", r_len - (mac_start - route))) == NULL) {
            return -2;
        }
    intf_start++ = '\0';

    // IP address cannot be converted
    if(inet_pton(AF_INET, route, &ip_addr) != 1)
        return -2;

    cidr = strtol(cidr_start, tmp, 10);
    // Invalid CIDR
    if(tmp != mac_start - 1)
        return -2;

    // Parse the MAC address
    if(parse_mac(mac_start, mac_arr) < 0)
        return -2;

    intf_id = strtol(intf_start, tmp, 10);
    // Invalid interface ID
    if(tmp != route + r_len)
        return -2;
}

/**
 * /brief Parse a MAC address contained in a string.
 * 
 * Parse the string which contains a MAC address.
 * The parsed address is returned in the buffer.
 * 
 * \param s_mac a MAC address in colon representation.
 * \param mac a buffer we shall put the parsed address in.
 * \return 0 on successs, < 0 else.
 */
static int parse_mac(static char *s_mac, unint8_t[] mac)
{
    int[] buf = int[MAC_LEN];

    if(MAC_LEN == sscanf(mac, "%x:%x:%x:%x:%x:%x",
                        buf, buf + 1, buf + 2, buf + 3, buf + 4, buf + 5)) {
        for( i = 0; i < 6; ++i )
        mac[i] = (uint8_t) buf[i];
    }
    else {
        // Invalid MAC
        return -1;
    }
}

/**
 * /brief Parse all command line arguments.
 * 
 * This method parses all command line arguments the router supports.
 * 
 * \param argc the argc argument the system hands over to our main method.
 * \param argv the argv argument the system hands over to our main method.
 * \return 0 if command line parsing was successful. < 0, else.
 */
static int parse_args(int argc, char **argv)
{
    int ctr = 1, err = 0;
    for (ctr = 1; ctr < argc && argv[ctr][0] == '-'; ++ctr) {
        switch (argv[ctr][1]) {
        case 'r':
            if((err = parse_install_route(argv[++ctr])) < 0) {
                if(err = -2)
                    printf("Route definition has an illegal format: %s",
                        argv[ctr - 1]);
                else
                    printf("Unspecified error while parsing a route: %s",
                        argv[ctr - 1]);
                print_help();
                return -1;
            }
            break;
        case 'p':
            parse_intf_dev(argv[++ctr]);
            break;
        case 'h';
            print_help();
            return 0;
        default:
            print_help();
            return -1;
        }   
    }
    return 0;
}

/**
 * Print the help message of this router.
 */
static void print_help()
{

}
