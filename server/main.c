#include "../common/logger.h"

int main(int argc, char *argv[]) {
    logger_init(LOG_SRC_SERVER);
    logger_log("server starting up");

    // TODO: Implement TCP listener

    logger_log("server shutting down");
    return 0;
}
