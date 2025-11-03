#ifndef SERVER_WORKER_H
#define SERVER_WORKER_H

#include "../common/protocol.h"

#include <netinet/in.h>

typedef struct {
    // Client's RTSP (TCP) socket
    int rtsp_socket_fd;
    struct sockaddr_in client_addr;

    // Client's state
    client_state_t state;
    int session_id;

    // Client's RTP (UDP) port
    int rtp_port;
} session_t;

void *server_worker_thread(void *arg);

#endif // SERVER_WORKER_H
