#include "router.h"

/**
 * Main function of the router.
 */
int main(int argc, char* argv[]) {
    if(parse_args(argc, argv) < 0)
        return -1;
    return start_router() < 0 ? -1 : 0;
}

