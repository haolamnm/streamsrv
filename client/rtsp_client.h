#ifndef RTSP_CLIENT_H
#define RTSP_CLIENT_H

#include "../common/protocol.h"

#include <netinet/in.h>

typedef struct {
    int rtsp_socket_fd;
    struct sockaddr_in server_addr;

    int rtsp_seq;       // CSeq number
    int session_id;     // Session ID from server
    client_state_t state;
} rtsp_client_t;

int rtsp_client_connect(rtsp_client_t *client, const char *server_ip, int server_port);

int rtsp_client_send_setup(rtsp_client_t *client, const char *filename, int rtp_port);

rtsp_status_t rtsp_client_receive_reply(rtsp_client_t *client);

void rtsp_client_disconnect(rtsp_client_t *client);

#endif // RTSP_CLIENT_H
