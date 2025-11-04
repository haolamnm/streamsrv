#include "../common/logger.h"
#include "rtp_client.h"
#include "rtsp_client.h"
#include "client_ui.h"

#include <raylib.h>
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

    rtp_client_t rtp;
    memset(&rtp, 0, sizeof(rtp_client_t));

    client_ui_t ui;
    memset(&ui, 0, sizeof(client_ui_t));

    SetTraceLogLevel(LOG_ERROR);

    // Pass the un-connected client struct to the UI
    // The UI will be responsible for triggering the connection when the user clicks "SETUP"
    client_ui_init(&ui, &client, &rtp, server_ip, server_port, rtp_port, video_file);

    logger_log("running ui loop...");
    client_ui_run(&ui); // blocks until the window is closed

    logger_log("ui loop exited, cleaning up");
    client_ui_cleanup(&ui);

    logger_log("client shutting down");
    return EXIT_SUCCESS;
}
