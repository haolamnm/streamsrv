#ifndef RTP_CLIENT_H
#define RTP_CLIENT_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_BUFFER_SIZE 128000

typedef struct {
    uint8_t frame_buffer[FRAME_BUFFER_SIZE];
    size_t frame_size;
    int is_new; // 1 if new frame, 0 if ui seen it already
    pthread_mutex_t mutex;
} frame_buffer_t;

typedef struct {
    int rtp_socket_fd;
    pthread_t listen_thread_id;
    int stop_thread;

    frame_buffer_t frame;
} rtp_client_t;

int rtp_client_open_port(rtp_client_t *rtp, int port);

int rtp_client_start_listener(rtp_client_t *rtp);

size_t rtp_client_get_frame(rtp_client_t *rtp, uint8_t *out_buffer);

void rtp_client_stop_listener(rtp_client_t *rtp);

#endif // RTP_CLIENT_H
