#ifndef RTSP_CLIENT_H
#define RTSP_CLIENT_H

#include "../common/protocol.h"

#include <netinet/in.h>
#include <pthread.h>

typedef struct {
    int rtsp_socket_fd;
    struct sockaddr_in server_addr;

    int rtsp_seq;       // CSeq number
    int session_id;     // Session ID from server
    client_state_t state;

    char video_file[256];
    int rtp_port;
    pthread_t reply_thread_id; // thread to listen for replies
    int stop_reply_thread;
    pthread_mutex_t state_mutex;     // mutex to protect state

} rtsp_client_t;

int rtsp_client_connect(
    rtsp_client_t *client,
    const char *server_ip,
    int server_port,
    const char *filename,
    int rtp_port
);

int rtsp_client_start_reply_listener(rtsp_client_t *client);

int rtsp_client_send_setup(rtsp_client_t *client);
int rtsp_client_send_play(rtsp_client_t *client);
int rtsp_client_send_pause(rtsp_client_t *client);
int rtsp_client_send_teardown(rtsp_client_t *client);

void rtsp_client_disconnect(rtsp_client_t *client);

#endif // RTSP_CLIENT_H
