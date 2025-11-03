#include "../common/logger.h"
#include "../common/rtp_packet.h"
#include "server_worker.h"
#include "rtsp_parser.h"
#include "video_stream.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#define RECV_BUFFER_SIZE 2048
#define SEND_BUFFER_SIZE 1024

// 64KB is a safe size for a single MJPEG frame
#define FRAME_BUFFER_SIZE 65536
#define RTP_PACKET_BUFFER_SIZE (FRAME_BUFFER_SIZE + RTP_HEADER_SIZE)

static void *send_rtp_thread(void *arg) {
    session_t *session = (session_t *)arg;

    uint8_t frame_buffer[FRAME_BUFFER_SIZE];
    uint8_t rtp_buffer[RTP_PACKET_BUFFER_SIZE];

    // Set up the client's UDP address struct for sendto
    struct sockaddr_in rtp_addr;
    memset(&rtp_addr, 0, sizeof(rtp_addr));
    rtp_addr.sin_family = AF_INET;
    rtp_addr.sin_addr = session->client_addr.sin_addr;
    rtp_addr.sin_port = htons(session->rtp_port);

    logger_log("rtp sending thread started, target %s:%d",
        inet_ntoa(rtp_addr.sin_addr),
        ntohs(rtp_addr.sin_port)
    );

    struct timespec wait_time;
    struct timeval now;

    // Lock the mutex, the C-idiom way for a condition-variable-controlled-loop
    pthread_mutex_lock(&session->event_mutex);

    // Loop as long as the stop_rtp_thread is 0
    while (session->stop_rtp_thread == 0) {
        // Unlock -> Send -> Re-lock

        // Unlock
        pthread_mutex_unlock(&session->event_mutex);

        // Read frame
        ssize_t frame_size = video_stream_next_frame(
            &session->video_stream, frame_buffer, FRAME_BUFFER_SIZE
        );
        if (frame_size <= 0) {
            logger_log("end of video stream or read error");
            break;
        }

        // Encode
        size_t packet_size = rtp_packet_encode(
            rtp_buffer, RTP_PACKET_BUFFER_SIZE,
            2, 0, 0, 0, // version, padding, extension, cc
            (&session->video_stream)->frame_num,
            0, MJPEG_TYPE, 0, // marker, pt, ssrc
            frame_buffer, frame_size
        );

        // Send
        if (packet_size > 0) {
            ssize_t sent = sendto(
                session->rtp_socket_fd, rtp_buffer, packet_size, 0,
                (struct sockaddr *)&rtp_addr, sizeof(rtp_addr)
            );
            if (sent < 0) {
                logger_log("error sending rtp packet: %s", strerror(errno));
            }
        }

        // Wait 50ms for 20 FPS video
        gettimeofday(&now, NULL);

        // Convert us to ns by multiplying by 1000
        wait_time.tv_sec = now.tv_sec;
        wait_time.tv_nsec = (now.tv_usec + 50000) * 1000;

        // Handle nanosecond overflow (if > 1 billion)
        if (wait_time.tv_nsec >= 1000000000) {
            wait_time.tv_sec++;
            wait_time.tv_nsec -= 1000000000;
        }

        // Re-lock
        pthread_mutex_lock(&session->event_mutex);

        // This will wait until "wait_time" OR until signaled by another thread (e.g. PAUSE)
        // It will automatically unlocks the mutex, waits and re-locks it
        pthread_cond_timedwait(&session->event_cond, &session->event_mutex, &wait_time);
    }

    // Unclock one last time before exiting
    pthread_mutex_unlock(&session->event_mutex);

    logger_log("rtp sending thread stopping");
    return NULL;
}

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
        // session->session_id = (rand() % 899999) + 100000;
        session->session_id = 0;
    }

    // Initialize things
    session->state = STATE_READY;
    pthread_mutex_init(&session->event_mutex, NULL);
    pthread_cond_init(&session->event_cond, NULL);
    session->stop_rtp_thread = 1;
    session->rtp_socket_fd = -1;

    send_rtsp_reply(session, STATUS_OK_200, info->cseq);
}

static void handle_play(session_t *session, rtsp_request_info_t *info) {
    handle_setup(session, info);

    if (session->state != STATE_READY) {
        logger_log("received play in non-ready state");
        return;
    }

    logger_log("processing play");

    // Create the UDP socket
    session->rtp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (session->rtp_socket_fd < 0) {
        logger_log("error creating rtp socket: %s", strerror(errno));
        send_rtsp_reply(session, STATUS_SRV_ERR_500, info->cseq);
        return;
    }

    session->state = STATE_PLAYING;

    // Send the reply before starting the thread
    send_rtsp_reply(session, STATUS_OK_200, info->cseq);

    pthread_mutex_lock(&session->event_mutex);
    session->stop_rtp_thread = 0; // set to play
    pthread_mutex_unlock(&session->event_mutex);

    if (pthread_create(&session->rtp_thread_id, NULL, send_rtp_thread, (void *)session) != 0) {
        logger_log("error creating rtp thread");
        session->state = STATE_READY; // revert state
    }
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
        handle_play(session, &info);
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

    // Signal the RTP thread to stop then wait for it to exit
    if (session->state == STATE_PLAYING) {
        pthread_mutex_lock(&session->event_mutex);
        session->stop_rtp_thread = 1;

        // Wake it up if it is in cond_timedwait
        pthread_cond_signal(&session->event_cond);
        pthread_mutex_unlock(&session->event_mutex);

        // Wait for RTP thread to finish
        logger_log("waiting for rtp thread to join...");
        pthread_join(session->rtp_thread_id, NULL);
        logger_log("rtp thread joined");
    }

    // Clean up resources
    pthread_mutex_destroy(&session->event_mutex);
    pthread_cond_destroy(&session->event_cond);
    video_stream_close(&session->video_stream);

    // Clean up socket
    if (session->rtp_socket_fd > 0) {
        close(session->rtp_socket_fd);
    }
    close(session->rtsp_socket_fd);

    free(session);
    return NULL;
}
