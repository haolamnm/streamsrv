#include "../common/logger.h"
#include "rtsp_client.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define RECV_BUFFER_SIZE 1024
#define SEND_BUFFER_SIZE 1024

static rtsp_status_t process_rtsp_reply(rtsp_client_t *client, const char *reply_str) {
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

    // Check status code
    if (status_code == 200) {
        pthread_mutex_lock(&client->state_mutex);

        if (client->state == STATE_INIT) {
            client->session_id = session_id;
            client->state = STATE_READY;
            logger_log("state changed to READY from INIT");
        } else if (client->state == STATE_READY) {
            client->state = STATE_PLAYING;
            logger_log("state changed to PLAYING from READY");
        } else if (client->state == STATE_PLAYING) {
            client->state = STATE_READY;
            logger_log("state changed to READY from PAUSE");
        }
        pthread_mutex_unlock(&client->state_mutex);
        return STATUS_OK_200;
    } else {
        logger_log("server returned error %d", status_code);
        return STATUS_SRV_ERR_500;
    }
}

static void *rtsp_reply_thread(void* arg) {
    rtsp_client_t *client = (rtsp_client_t*)arg;
    char recv_buffer[RECV_BUFFER_SIZE];

    logger_log("rtsp reply thread started");

    while (client->stop_reply_thread == 0) {
        ssize_t bytes_read = read(client->rtsp_socket_fd, recv_buffer, RECV_BUFFER_SIZE - 1);

        if (bytes_read <= 0) {
            if (client->stop_reply_thread != 0) {
                break;
            }
            logger_log("server disconnected or read error");
            break;
        }
        recv_buffer[bytes_read] = '\0';
        process_rtsp_reply(client, recv_buffer);
    }

    logger_log("rtsp reply thread stopping");
    return NULL;
}

int rtsp_client_connect(
    rtsp_client_t *client,
    const char *server_ip,
    int server_port,
    const char *filename,
    int rtp_port
) {
    memset(client, 0, sizeof(rtsp_client_t));
    client->rtsp_socket_fd = -1;
    client->rtsp_seq = 0;
    client->session_id = 0;
    client->state = STATE_INIT;
    client->stop_reply_thread = 1;

    // Store args for SETUP request
    strncpy(client->video_file, filename, sizeof(client->video_file) - 1);
    client->rtp_port = rtp_port;

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

int rtsp_client_start_reply_listener(rtsp_client_t *client) {
    client->stop_reply_thread = 0;
    if (pthread_create(&client->reply_thread_id, NULL, rtsp_reply_thread, (void *)client) != 0) {
        logger_log("error creating rtsp reply thread");
        return -1;
    }
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

int rtsp_client_send_setup(rtsp_client_t *client) {
    char send_buffer[SEND_BUFFER_SIZE];
    client->rtsp_seq++;

    sprintf(send_buffer, "SETUP %s %s\r\nCSeq: %d\r\nTransport: RTP/UDP;client_port=%d\r\n\r\n",
        client->video_file, RTSP_VERSION, client->rtsp_seq, client->rtp_port
    );
    return send_rtsp_request(client, send_buffer);
}

int rtsp_client_send_play(rtsp_client_t *client) {
    char send_buffer[SEND_BUFFER_SIZE];
    client->rtsp_seq++;

    sprintf(send_buffer, "PLAY %s %s\r\nCSeq: %d\r\nSession: %d\r\n\r\n",
            client->video_file, RTSP_VERSION, client->rtsp_seq, client->session_id);

    return send_rtsp_request(client, send_buffer);
}

int rtsp_client_send_pause(rtsp_client_t *client) {
    char send_buffer[SEND_BUFFER_SIZE];
    client->rtsp_seq++;

    sprintf(send_buffer, "PAUSE %s %s\r\nCSeq: %d\r\nSession: %d\r\n\r\n",
            client->video_file, RTSP_VERSION, client->rtsp_seq, client->session_id);

    return send_rtsp_request(client, send_buffer);
}

int rtsp_client_send_teardown(rtsp_client_t *client) {
    char send_buffer[SEND_BUFFER_SIZE];
    client->rtsp_seq++;

    sprintf(send_buffer, "TEARDOWN %s %s\r\nCSeq: %d\r\nSession: %d\r\n\r\n",
            client->video_file, RTSP_VERSION, client->rtsp_seq, client->session_id);

    return send_rtsp_request(client, send_buffer);
}

void rtsp_client_disconnect(rtsp_client_t *client) {
    logger_log("disconnecting...");

    if (client->stop_reply_thread == 0) {
        client->stop_reply_thread = 1;
        // Close the socket, which will unblock the read()  in the reply thread
        // causing it to exit
        if (client->rtsp_socket_fd > 0) {
            close(client->rtsp_socket_fd);
        }
        pthread_join(client->reply_thread_id, NULL);
        logger_log("rtsp reply thread joined");
    }
    pthread_mutex_destroy(&client->state_mutex);

    logger_log("disconnected from server");
}
