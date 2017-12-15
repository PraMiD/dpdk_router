#include "router.h"

/**
 * Main function of the router.
 */
int main(int argc, char* argv[]) {
    switch(parse_args(argc, argv)) {
        case 0:
            return start_router() < 0 ? -1 : 0;
        case 1:
            return 0; // Help printed
        default: // All errors
            return -1;
    }
}

