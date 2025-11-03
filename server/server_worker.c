#include "../common/logger.h"
#include "server_worker.h"
#include "rtsp_parser.h"
#include "video_stream.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RECV_BUFFER_SIZE 2048
#define SEND_BUFFER_SIZE 1024

static void send_rtsp_reply(session_t *session, rtsp_status_t status, int cseq) {
    char send_buffer[SEND_BUFFER_SIZE];
    char status_str[64];

    switch (status) {
    case STATUS_OK_200:
        strcpy(status_str, "200 OK");
        break;
    case STATUS_NOT_FOUND_404:
        strcpy(status_str, "404 Not Found");
        break;
    case STATUS_SRV_ERR_500:
    default:
        strcpy(status_str, "500 Internal Server Error");
        break;
    }

    // The session ID will be sent in every reply
    sprintf(send_buffer, "%s %s\r\nCSeq: %d\r\nSession: %d\r\n\r\n",
        RTSP_VERSION, status_str, cseq, session->session_id
    );
    logger_log("sending reply:\n%s", send_buffer);

    if (send(session->rtsp_socket_fd, send_buffer, strlen(send_buffer), 0) < 0) {
        logger_log("error sending reply: %s", strerror(errno));
    }
}

static void handle_setup(session_t *session, rtsp_request_info_t *info) {
    if (session->state != STATE_INIT) {
        logger_log("received setup in non-init state");
        return;
    }

    logger_log("processing setup for file: %s", info->filename);

    // Try to open the video file
    if (video_stream_open(&session->video_stream, info->filename) != 0) {
        logger_log("file not found: %s", info->filename);
        send_rtsp_reply(session, STATUS_NOT_FOUND_404, info->cseq);
        return;
    }

    logger_log("video stream opened successfully");

    // Now the file exists, store the request details
    strncpy(session->filename, info->filename, sizeof(session->filename));
    session->rtp_port = info->rtp_port;

    // Generate a random session ID for the client
    if (session->session_id == 0) {
        session->session_id = (rand() % 899999) + 100000;
    }

    session->state = STATE_READY;
    send_rtsp_reply(session, STATUS_OK_200, info->cseq);
}

static void process_rtsp_request(session_t *session, char *request) {
    rtsp_request_info_t info;
    rtsp_parse_request(request, &info);

    // Check if the session ID match, unless it is a SETUP request
    if (info.method != METHOD_SETUP && info.session_id != session->session_id) {
        logger_log("session id mismatch. expected %d, got %d",
            session->session_id, info.session_id
        );
        return;
    }

    // Route the request based on the parsed method
    switch (info.method) {
    case METHOD_SETUP:
        handle_setup(session, &info);
        break;
    case METHOD_PLAY:
        logger_log("play request received (not implemented)");
        // TODO
        break;
    case METHOD_PAUSE:
        logger_log("pause request received (not implemented)");
        // TODO
        break;
    case METHOD_TEARDOWN:
        logger_log("teardown request received (not implemented");
        // TODO
        break;
    default:
        logger_log("received unknown or malformed request");
        // TODO
        break;
    }
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
    video_stream_close(&session->video_stream);
    close(session->rtsp_socket_fd);
    free(session);
    return NULL;
}
