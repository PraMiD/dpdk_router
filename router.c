#include <arpa/inet.h>
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

#include "router.h"
#include "dpdk_init.h"
#include "routing_table.h"
#include "ethernet_stack.h"
#include "global.h"

// A route in the given format <IP>/<CIDR>,<MAC>,<interface>
// must not be longer than 36 characters at max
#define MAC_LEN ETHER_ADDR_LEN


/**********************************
 *  Static function declarations  *
 **********************************/
static int parse_install_route(const char *route);
static int parse_intf_dev(const char *def);
static int parse_mac(const char *s_mac, struct ether_addr *mac);
static int cfg_intfs();
static int dpdk_init();
static int start_threads();
static int router_thread(void *arg);
static void print_help();


/**********************************
 *       Lists and Fields         *
 **********************************/
static uint no_intf = 0;
intf_cfg_t *intf_cfgs = NULL;

int router_thread(void *arg)
{
    intf_cfg_t* cfg = (intf_cfg_t *)arg;
	struct rte_mbuf* buf[THREAD_BUFSIZE];

	while (1) {
		uint32_t rx = recv_from_device(cfg->intf, cfg->num_rx_queues, buf, THREAD_BUFSIZE);
        if (rx == 0)
            usleep(100);
		for (uint32_t i = 0; i < rx; ++i) {
            handle_frame(cfg, buf[i]);
        }
	}
	return 0;
}

/*********************************
 *      Function definitions     *
 *********************************/
/**
 * \brief Start the router.
 * 
 * Start the router by initializing the DPDK framework, configuring all
 * interfaces and starting the single threads that handle the packets on the
 * configured interfaces.
 * 
 * \return 0 on success.
 *          Errors: ERR_GEN
 */
int start_router()
{
    if(dpdk_init() < 0) {
        printf("Not able to initialize the router. Aborting...\n");
        printf("rte_erro while dpdk_init: %d\n", rte_errno);
        return ERR_GEN;
    }

    if(cfg_intfs() < 0) {
        printf("Could not configure the interfaces! Aborting...\n");
        return ERR_GEN;
    }

    printf("Starting to serve on %d interfaces!\n", no_intf);

    start_threads();

    // Wait until all lcores have finished serving
    rte_eal_mp_wait_lcore();
    return 0;
}

/**
 * \brief Start packet processing on the different interfaces.
 * 
 * This functions starts the packet processing on the different lcores.
 * We pass the intf_cfg_t structure to the router_thread method responsible
 * for this interface as arguments.
 * 
 * \return 0 on success.
 *          Errors: ERR_START If we could not start the thread on one core.
 */
static int start_threads() {
    // lcore 0 is reserved for the MASTER
    int it = 1;
    intf_cfg_t *iterator = intf_cfgs;

    for(; iterator != NULL; iterator = iterator->nxt, it++) {
        iterator->num_rx_queues = no_intf;
        iterator->lcore = it;
        rte_eth_macaddr_get(iterator->intf, &iterator->ether_addr);

        
        if(rte_eal_remote_launch(router_thread, iterator, it) < 0) {
            printf("Could not launch packet processing on lcore %d\n", it);
            return ERR_START;
        }
        printf("Starting to process packets of interface: %d on lcore %d\n",
                iterator->intf, iterator->lcore);
    }
    return 0;
}

/**
 * \brief Add a new interface configuration to the list of
 *      interface configurations.
 * 
 * Adds a new interface configuration for the given interface and ip address
 * to the list of interface configurations.
 * 
 * \param intf ID of the interface this config belongs to.
 * \param ip_addr IPv4 address of the interface in big endian format.
 * \return 0 on success.
 *      Errors: ERR_MEM
 */
int add_intf_cfg(uint8_t intf, uint32_t ip_addr) {
    intf_cfg_t **iterator = &intf_cfgs;

    while(*iterator != NULL)
        iterator = &((*iterator)->nxt);

    if((*iterator = malloc(sizeof(intf_cfg_t))) == NULL)
        return ERR_MEM;

    (*iterator)->ip_addr_be = ip_addr;
    (*iterator)->intf = intf;
    (*iterator)->nxt = NULL;

    no_intf++;

    printf("Added interface configuration for interface %d\n", intf);
    return 0;
}


/*********************************
 *  Static function definitions  *
 *********************************/
/**
 * \brief Initialize DPDK.
 * 
 * Initialize DPDK by setting the number of cores to use, the number of memory
 * sockets and the coremask.
 * We will reserve no_intf + 1 threads for the router as lcore 0 is for the 
 * master -> Therefore, we can use 1..no_intf + 1 for the clients!
 * 
 * \return 0 if the initilaization was successful.
 *          Errors: ERR_CFG: Some error occured while configuring DPDK.
 */
static int dpdk_init()
{
    int argc = no_intf > 0 ? 3 : 2;
	char* argv[argc];
    char no_cores[32];

    argv[0] = "-c1";
    argv[1] = "-n1";

    if(no_intf > 0) {
        // +1 as we need one for the master thread (ID 0)
        sprintf(no_cores, "--lcores=(0-%1.3u)@0", no_intf+1);
        argv[2] = no_cores;    
    }

	if(rte_eal_init(argc, argv) == -1)
        return ERR_CFG;
    return 0;
}

/**
 * \brief Setup the interfaces the user entered using the command line options.
 * 
 * Configure the interfaces used by this router. We will configure the
 * interfaces according to the intf_cfgs list.
 * 
 * \return 0 if interface configuration was successful for all interfaces.
 *          Errors: Currently none, but we use int as return type if we
 *                  want to use error messages in the future.
 *                  Therefore, the user should test if the return value
 *                  is smaller than zero for upwards compatibility.
 */
static int cfg_intfs()
{
    intf_cfg_t *iterator = intf_cfgs;
    
    for(; iterator != NULL; iterator = iterator->nxt) {
        configure_device(iterator->intf, no_intf);
    }

    return 0;
}

/**
 * \brief Parse a single route and add it to the routing table.
 * 
 * This method will parse a route given as command line argument to the router.
 * After checking the format, we add it to the routing table.
 * Routing definition example: 10.0.10.2/32,52:54:00:cb:ee:f4,0
 * Format: <net_address>/prefix,<nxt_hop_max>,<egress_iface>
 * 
 * This function is designed to work on command line arguments.
 * In addition, we do not copy the input string to some local buffer.
 * Therefore, (and because we do not have access to strnstr.. because
 * compiliation is done using the C99 standard) we do not check the length of
 * the command line arguments.
 * 
 * \param route a string containing a single route as command line argument.
 * \return 0 if we could parse the route.
 *          Errors: ERR_FORMAT
 */
static int parse_install_route(const char *route)
{
    char *cidr_start = NULL, *mac_start = NULL, *intf_start = NULL, *tmp = NULL;
    uint32_t net_addr = 0;
    uint8_t cidr = 0, intf_id = 0;
    long ltmp = 0;
    struct ether_addr mac_addr;

    // Missing CIDR
    if((cidr_start = strstr(route, "/")) == NULL)
        return ERR_FORMAT;
    *cidr_start = '\0';
    cidr_start++;

    // Missing MAC
    if((mac_start = 
        strstr(cidr_start, ",")) == NULL) {
            return ERR_FORMAT;
        }
    *mac_start = '\0';
    mac_start++;

    // Missing MAC
    if((intf_start = 
        strstr(mac_start, ",")) == NULL) {
            return ERR_FORMAT;
        }
    *intf_start = '\0';
    intf_start++;

    // IP address cannot be converted
    if(inet_pton(AF_INET, route, &net_addr) != 1)
        return ERR_FORMAT;

    ltmp = strtol(cidr_start, &tmp, 10);
    // Invalid CIDR
    if(tmp != mac_start - 1)
        return ERR_FORMAT;
    if(((uint8_t)ltmp) != ltmp)
        return ERR_FORMAT;
    cidr = (uint8_t)ltmp;

    // Parse the MAC address
    if(parse_mac(mac_start, &mac_addr) < 0)
        return ERR_FORMAT;

    ltmp = strtol(intf_start, &tmp, 10);
    // Invalid interface ID
    if(*tmp != '\0')
        return ERR_FORMAT;
    if(((uint8_t)ltmp) != ltmp)
        return ERR_FORMAT;
    intf_id = (uint8_t)ltmp;

    install_route(net_addr, cidr, intf_id, &mac_addr);

    return 0;
}


/**
 * /brief Parse a single interface definition and add it to the interface config.
 * 
 * This method parses a interface definition of the given format:
 *      <intf_id>,<ip_address>
 * After successful parsing, we add a new interface configuration structure
 * to the list of configurations.
 * 
 * This function is designed to work on command line arguments.
 * In addition, we do not copy the input string to some local buffer.
 * Therefore, (and because we do not have access to strnstr.. because
 * compiliation is done using the C99 standard) we do not check the length of
 * the command line arguments.
 * 
 * \param def a string containing a single interface definition.
 * \return 0 if we could parse the interface definition.
 *          Errors: ERR_FORMAT, ERR_MEM, ERR_GEN
 */
static int parse_intf_dev(const char *def)
{
    long ltmp = 0;
    uint8_t intf = 0;
    uint32_t ip_addr = 0;
    char *ip_start = NULL, *tmp = NULL;

    // Get the seperating ','
    if((ip_start = strstr(def, ",")) == NULL)
        return ERR_FORMAT;
    *ip_start = '\0';
    ip_start++;
    
    ltmp = strtol(def, &tmp, 10);
    // Invalid interface ID
    if(*tmp != '\0')
        return ERR_FORMAT;
    if(((uint8_t)ltmp) != ltmp)
        return ERR_FORMAT;
    intf = (uint8_t)ltmp;
    if(inet_pton(AF_INET, ip_start, &ip_addr) != 1)
        return ERR_FORMAT;
    

    return add_intf_cfg(intf, ip_addr);
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
 * \return 0 if command line parsing was successful.
 *      Errors: ERR_GEN
 */
int parse_args(int argc, char **argv)
{
    int ctr = 1, err = 0;
    for (ctr = 1; ctr < argc && argv[ctr][0] == '-'; ++ctr) {
        switch (argv[ctr][1]) {
        case 'r':
            if(parse_install_route(argv[++ctr]) < 0) {
                printf("Route definition has an illegal format!\n");
                return ERR_GEN;
            }
            break;
        case 'p':
            if((err = parse_intf_dev(argv[++ctr])) < 0) {
                printf("Error: %d\n", err);
                if(err == ERR_GEN)
                    printf("Could not parse the interface configuration because"
                        "of an unknown error!\n");
                if(err == ERR_FORMAT)
                    printf("Interface configuration has an illegal format:"
                        "'%s'\n", argv[ctr]);
                else if (err == ERR_MEM)
                    printf("Could not add interface specification."
                        "Out of memory!\n");
                return ERR_GEN;
            }
            break;
        case 'h':
            print_help();
            return 0;
        default:
            print_help();
            return ERR_GEN;
        }   
    }
    if(no_intf == 0)
                printf("Warning:"
                    "No interfaces specified the router shall handle.\n");
    return 0;
}

/**
 * Print the help message of this router.
 */
static void print_help()
{

}