#include "../common/logger.h"

#include <stdlib.h>

int main(int argc, char *argv[]) {
    logger_init(LOG_SRC_CLIENT);
    logger_log("client starting up");

    // TODO: Implement connection to the server

    logger_log("client shutting down");
    return EXIT_SUCCESS;
}
