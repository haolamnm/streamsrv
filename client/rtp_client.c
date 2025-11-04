#include "../common/logger.h"
#include "../common/rtp_packet.h"
#include "rtp_client.h"

#include <asm-generic/socket.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>

#define RTP_RECV_BUFFER_SIZE (FRAME_BUFFER_SIZE + 512)

static void *rtp_listen_thread(void *arg) {
    rtp_client_t *rtp = (rtp_client_t *)arg;
    uint8_t recv_buffer[RTP_RECV_BUFFER_SIZE];

    logger_log("rtp listen thread started");

    while (rtp->stop_thread == 0) {
        // Blocking here, wait for UDP packet
        ssize_t bytes_read = recvfrom(
            rtp->rtp_socket_fd, recv_buffer, RTP_RECV_BUFFER_SIZE, 0, NULL, NULL
        );
        if (bytes_read <= 0) {
            if (rtp->stop_thread != 0) {
                break;
            }
            logger_log("rtp recv error: %s", strerror(errno));
            continue;
        }

        // Received a packet, now decode it
        rtp_header_t header;
        uint8_t *payload;
        size_t payload_size = rtp_packet_decode(&header, &payload, recv_buffer, bytes_read);

        if (payload_size > 0) {
            // Lock the thread to safely write to the shared buffer
            pthread_mutex_lock(&rtp->frame.mutex);

            if (payload_size <= FRAME_BUFFER_SIZE) {
                memcpy(rtp->frame.frame_buffer, payload, payload_size);
                rtp->frame.frame_size = payload_size;
                rtp->frame.is_new = 1;
            } else {
                logger_log("frame is too large for buffer");
            }
            pthread_mutex_unlock(&rtp->frame.mutex);
        }
    }
    logger_log("rtp listen thread stopping");
    return NULL;
}

int rtp_client_open_port(rtp_client_t *rtp, int port) {
    pthread_mutex_init(&rtp->frame.mutex, NULL);
    rtp->frame.frame_size = 0;
    rtp->frame.is_new = 0;
    rtp->stop_thread = 1;
    rtp->rtp_socket_fd = -1;

    // Start a UDP connection socket
    rtp->rtp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtp->rtp_socket_fd < 0) {
        logger_log("error creating rtp socket: %s", strerror(errno));
        return -1;
    }

    // Set a short timeout on socket, allow the thread loop to check for "stop_thread"
    // instead of blocking forever
    struct timeval tv;
    tv.tv_sec = 1; // 1 sec timeout
    tv.tv_usec = 0;
    setsockopt(rtp->rtp_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Bind the socket to the port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(rtp->rtp_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        logger_log("error binding rtp socket: %s", strerror(errno));
        close(rtp->rtp_socket_fd);
        return -1;
    }
    logger_log("rtp port opened and bound to %d", port);
    return 0;
}

int rtp_client_start_listener(rtp_client_t *rtp) {
    rtp->stop_thread = 0;
    if (pthread_create(&rtp->listen_thread_id, NULL, rtp_listen_thread, (void *)rtp) != 0) {
        logger_log("error creating rtp listen thread");
        return -1;
    }
    return 0;
}

size_t rtp_client_get_frame(rtp_client_t *rtp, uint8_t *out_buffer) {
    size_t frame_size = 0;

    // Lock mutex to check the shared buffer
    pthread_mutex_lock(&rtp->frame.mutex);

    if (rtp->frame.is_new != 0) {
        // Copy this new frame into the UI buffer
        memcpy(out_buffer, rtp->frame.frame_buffer, rtp->frame.frame_size);
        frame_size = rtp->frame.frame_size;
        rtp->frame.is_new = 0;
    }
    pthread_mutex_unlock(&rtp->frame.mutex);

    return frame_size;
}

void rtp_client_stop_listener(rtp_client_t *rtp) {
    logger_log("stopping rtp listener...");
    if (rtp->stop_thread == 0) {
        rtp->stop_thread = 1;
        pthread_join(rtp->listen_thread_id, NULL);
        logger_log("rtp thread joined");
    }
    if (rtp->rtp_socket_fd > 0) {
        close(rtp->rtp_socket_fd);
    }
    pthread_mutex_destroy(&rtp->frame.mutex);
}
