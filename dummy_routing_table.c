#include "routing_table.h"

#include <rte_config.h>
#include <rte_ip.h>

static struct routing_table_entry hop_info1 = {
    .dst_mac = {.addr_bytes = {0x52, 0x54, 0x00, 0x61, 0x7a, 0x13}},
    .dst_port = 0
};
static struct routing_table_entry hop_info2 = {
    .dst_mac = {.addr_bytes = {0x52, 0x54, 0x00, 0xc4, 0x4f, 0xfa}},
    .dst_port = 1
};

struct routing_table_entry* get_next_hop(uint32_t ip) {
	if (ip == rte_cpu_to_be_32(IPv4(10,0,0,2))) {
		return &hop_info1;
	} else if (ip == rte_cpu_to_be_32(IPv4(192,168,0,2))) {
		return &hop_info2;
	} else {
		return NULL;
	}
}
