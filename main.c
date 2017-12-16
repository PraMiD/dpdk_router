#include "router.h"

/**
 * Main function of the router.
 */
int main(int argc, char* argv[]) {
    int err = parse_args(argc, argv);

    switch(err) {
        case 0:
            // Should never return but it is nicer that way
            err = start_router() < 0 ? -1 : 0; 
            break;
        case 1: // Help printed
            err = 0;
            break;
        default:
            break;
    }

    clean_shutdown();
    return err;
}

