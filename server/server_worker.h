#ifndef SERVER_WORKER_H
#define SERVER_WORKER_H

#include "../common/protocol.h"
#include "video_stream.h"

#include <netinet/in.h>
#include <pthread.h>

typedef struct {
    // Client's RTSP (TCP) socket
    int rtsp_socket_fd;
    struct sockaddr_in client_addr;

    // Client's state
    client_state_t state;
    int session_id;

    // Client's RTP (UDP) port
    int rtp_port;

    // Video stream section
    video_stream_t video_stream;
    char filename[256];

    // RTP (UDP) socket for sending data
    int rtp_socket_fd;

    // Thread ID for RTP sending thread
    pthread_t rtp_thread_id;

    // Synchronization for RTP thread
    pthread_mutex_t event_mutex;
    pthread_cond_t event_cond;
    int stop_rtp_thread; // 0 = play, 1 = stop (for PAUSE/TEARDOWN)
    
    // RTP sequence number counter (independent from video file frame count)
    uint16_t rtp_seqnum;
} session_t;

void *server_worker_thread(void *arg);

#endif // SERVER_WORKER_H
