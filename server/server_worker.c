#include "../common/logger.h"
#include "server_worker.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RECV_BUFFER_SIZE 2048

static void process_rtsp_request(session_t *session, char *request) {
    logger_log("processing rtsp request...");

    // TODO: Parse request (SETUP, PLAY, PAUSE, TEARDOWN)
    // TODO: Send RTSP reply
}

void *server_worker_thread(void *arg) {
    session_t *session = (session_t *)arg;

    char buffer[RECV_BUFFER_SIZE];
    ssize_t bytes_read;

    logger_log("new client thread started");

    while(1) {
        // Blocking here, waiting for data from the client
        bytes_read = read(session->rtsp_socket_fd, buffer, RECV_BUFFER_SIZE - 1);

        if (bytes_read <= 0) {
            logger_log("client disconnected or read error");
            break;
        }
        buffer[bytes_read] = '\0';
        logger_log("received data:\n%s", buffer);
        process_rtsp_request(session, buffer);
    }
    logger_log("closing client connection and exiting thread");
    close(session->rtsp_socket_fd);
    free(session);
    return NULL;
}
