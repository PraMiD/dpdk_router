#ifndef GLOBAL_H__
#define GLOBAL_H__
/**
 * This file provides definitions we use in the entire program.
 */

// A route in the given format <IP>/<CIDR>,<MAC>,<interface>
// must not be longer than 36 characters at max
#define MAC_LEN 6

// Errors
#define ERR_GEN -1
#define ERR_FORMAT -2
#define ERR_MEM -3
#define ERR_ARG_NULL -4


#endif