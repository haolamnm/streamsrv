#include "../common/logger.h"
#include "rtsp_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    logger_init(LOG_SRC_CLIENT);

    if (argc != 5) {
        fprintf(stderr, "Usage: %s [srv_ip] [srv_port] [rtp_port] [file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int rtp_port = atoi(argv[3]);
    char *video_file = argv[4];

    logger_log("client starting up");

    rtsp_client_t client;
    memset(&client, 0, sizeof(rtsp_client_t));

    // Connect
    if (rtsp_client_connect(&client, server_ip, server_port) != 0) {
        logger_log("failed to connect to server");
        return EXIT_FAILURE;
    }

    // Send SETUP
    if (rtsp_client_send_setup(&client, video_file, rtp_port) != 0) {
        logger_log("failed to send setup request");
        rtsp_client_disconnect(&client);
        return EXIT_FAILURE;
    }

    // Wait for SETUP reply
    if (rtsp_client_receive_reply(&client) != STATUS_OK_200) {
        logger_log("server did not accept setup request");
        rtsp_client_disconnect(&client);
        return 1;
    }

    logger_log("setup done for session id: %d", client.session_id);

    // TODO: Implement UI

    rtsp_client_disconnect(&client);

    logger_log("client shutting down");
    return EXIT_SUCCESS;
}
