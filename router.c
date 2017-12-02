#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

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
#define MAC_LEN 6

/**********************************
 *  Static structure definitions  *
 **********************************/
typedef struct routing_table_line {
    uint32_t dip;
    uint32_t prf;
    struct routing_table_entry rte;
} routing_table_line;


/**********************************
 *  Static function declarations  *
 **********************************/
static int parse_install_route(const char *route);
static int parse_intf_dev(const char *def);
static int parse_mac(const char *s_mac, struct ether_addr *mac);
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
 * This function is designed to work on command line arguments.
 * In addition, we do not copy the input string to some local buffer.
 * Therefore, (and because we do not have access to strnstr.. because
 * compiliation is done using the C99 standard) we do not check the length of
 * the command line arguments.
 * 
 * \param route a string containing a single route as command line argument.
 * \return 0 if we could parse the route.
 *      -1 on a unspecified error,
 *      -2 if the route specification has an unknown format.
 *      -3 if there is not enough memory in the system.
 */
static int parse_install_route(const char *route)
{
    char *cidr_start = NULL, *mac_start = NULL, *intf_start = NULL, *tmp = NULL;
    uint32_t ip_addr = 0;
    uint8_t cidr = 0, intf_id = 0;
    long ltmp = 0;
    struct ether_addr mac_addr;

    // Missing CIDR
    if((cidr_start = strstr(route, "/")) == NULL)
        return -2;
    cidr_start = '\0';
    cidr_start++;

    // Missing MAC
    if((mac_start = 
        strstr(cidr_start, ",")) == NULL) {
            return -2;
        }
    mac_start = '\0';
    mac_start++;

    // Missing MAC
    if((intf_start = 
        strstr(cidr_start, ",")) == NULL) {
            return -2;
        }
    intf_start = '\0';
    intf_start++;

    // IP address cannot be converted
    if(inet_pton(AF_INET, route, &ip_addr) != 1)
        return -2;

    ltmp = strtol(cidr_start, &tmp, 10);
    // Invalid CIDR
    if(tmp != mac_start - 1)
        return -2;
    if(ltmp > ~cidr)
        return -2;
    cidr = (uint8_t)ltmp;

    // Parse the MAC address
    if(parse_mac(mac_start, &mac_addr) < 0)
        return -2;

    ltmp = strtol(intf_start, &tmp, 10);
    // Invalid interface ID
    if(*tmp != '\0')
        return -2;
    if(ltmp > ~intf_id)
        return -2;
    intf_id = (uint8_t)ltmp;

    add_route(ip_addr, cidr, &mac_addr, intf_id);
    return 0;
}

static int parse_intf_dev(const char *def)
{
    return 0;
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
static int parse_mac(const char *s_mac, struct ether_addr *mac)
{
    int buf[MAC_LEN];
    int ctr = 0;

    if(MAC_LEN == sscanf(s_mac, "%x:%x:%x:%x:%x:%x",
                        buf, buf + 1, buf + 2, buf + 3, buf + 4, buf + 5)) {
        for(ctr = 0; ctr < 6; ++ctr)
            mac->addr_bytes[ctr] = (uint8_t) buf[ctr];
        return 0;
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
int parse_args(int argc, char **argv)
{
    int ctr = 1, err = 0;
    for (ctr = 1; ctr < argc && argv[ctr][0] == '-'; ++ctr) {
        switch (argv[ctr][1]) {
        case 'r':
            if((err = parse_install_route(argv[++ctr])) < 0) {
                if(err == -2)
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
        case 'h':
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
