#include "../common/logger.h"
#include "rtsp_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define RECV_BUFFER_SIZE 1024
#define SEND_BUFFER_SIZE 1024

static rtsp_status_t parse_rtsp_reply(rtsp_client_t *client, const char *reply_str) {
    logger_log("recevied reply:\n%s", reply_str);

    char *buffer = strdup(reply_str);
    if (buffer == NULL) {
        return STATUS_SRV_ERR_500;
    }

    char *save_ptr;
    char *line = strtok_r(buffer, "\n", &save_ptr);

    char version[32];
    int status_code = 0;
    if (line != NULL) {
        sscanf(line, "%31s %d", version, &status_code);
    }

    int cseq = 0;
    int session_id = 0;

    while ((line = strtok_r(NULL, "\n", &save_ptr)) != NULL) {
        if (strlen(line) <= 1) {
            break;
        }
        if (sscanf(line, "CSeq: %d", &cseq) == 1) {
            // CSeq header found
        } else if (sscanf(line, "Session: %d", &session_id) == 1) {
            // Session header found
        }
    }

    free(buffer);

    // Validate the CSeq to ensure this reply matches the most recent request
    if (cseq != client->rtsp_seq) {
        logger_log("cseq mismatch. expected %d, got %d", client->rtsp_seq, cseq);
        return STATUS_SRV_ERR_500;
    }

    // Check status code
    if (status_code == 200) {
        if (client->session_id == 0) {
            client->session_id = session_id;
        }

        if (client->session_id != session_id) {
            logger_log("session id mismatch. expected %d, got %d",
                client->session_id, session_id
            );
            return STATUS_SRV_ERR_500;
        }
        return STATUS_OK_200;
    } else if (status_code == 404) {
        return STATUS_NOT_FOUND_404;
    } else {
        return STATUS_SRV_ERR_500;
    }
}

int rtsp_client_connect(rtsp_client_t *client, const char *server_ip, int server_port) {
    client->rtsp_socket_fd = -1;
    client->rtsp_seq = 0;
    client->session_id = 0;
    client->state = STATE_INIT;

    client->rtsp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->rtsp_socket_fd < 0) {
        logger_log("error creating socket: %s", strerror(errno));
        return -1;
    }

    struct hostent *server = gethostbyname(server_ip);
    if (server == NULL) {
        logger_log("error: no such host: %s", server_ip);
        close(client->rtsp_socket_fd);
        return -1;
    }

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    memcpy(&client->server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    client->server_addr.sin_port = htons(server_port);

    // Establish the TCP connection
    if (connect(client->rtsp_socket_fd, (struct sockaddr*)&client->server_addr, sizeof(client->server_addr)) < 0) {
        logger_log("error connecting to server: %s", strerror(errno));
        close(client->rtsp_socket_fd);
        return -1;
    }

    logger_log("connected to server %s:%d", server_ip, server_port);
    return 0;
}

static int send_rtsp_request(rtsp_client_t *client, const char *request_str) {
    logger_log("sending request:\n%s", request_str);

    if (send(client->rtsp_socket_fd, request_str, strlen(request_str), 0) < 0) {
        logger_log("error sending request: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int rtsp_client_send_setup(rtsp_client_t *client, const char *filename, int rtp_port) {
    if (client->state != STATE_INIT) {
        logger_log("setup already sent");
        return -1;
    }

    char send_buffer[SEND_BUFFER_SIZE];
    client->rtsp_seq++;

    sprintf(send_buffer, "SETUP %s %s\r\nCSeq: %d\r\nTransport: RTP/UDP;client_port=%d\r\n\r\n",
        filename, RTSP_VERSION, client->rtsp_seq, rtp_port
    );
    return send_rtsp_request(client, send_buffer);
}

rtsp_status_t rtsp_client_receive_reply(rtsp_client_t *client) {
    char recv_buffer[RECV_BUFFER_SIZE];

    // Blocking here, waiting for server reply
    ssize_t bytes_read = read(client->rtsp_socket_fd, recv_buffer, RECV_BUFFER_SIZE - 1);

    if (bytes_read <= 0) {
        logger_log("server disconnected or read error");
        return STATUS_SRV_ERR_500;
    }
    recv_buffer[bytes_read] = '\0';

    rtsp_status_t status = parse_rtsp_reply(client, recv_buffer);

    if (status == STATUS_OK_200 && client->state == STATE_INIT) {
        client->state = STATE_READY;
    }
    return status;
}

void rtsp_client_disconnect(rtsp_client_t *client) {
    if (client->rtsp_socket_fd > 0) {
        close(client->rtsp_socket_fd);
    }
    logger_log("disconnected from server");
}
