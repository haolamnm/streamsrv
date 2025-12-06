#ifndef RTP_CLIENT_H
#define RTP_CLIENT_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_BUFFER_SIZE 524288  // 512KB for FHD frames
#define CACHE_SIZE 20             // Pre-buffer frames

// Statistics for packet tracking
typedef struct {
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t frames_received;
    uint32_t frames_dropped;
    uint16_t last_seqnum;
    int first_packet;  // Flag for first packet
} rtp_stats_t;

// Single frame in the cache (heap allocated data)
typedef struct {
    uint8_t *data;      // Heap-allocated frame data
    size_t size;
    uint16_t seqnum;    // RTP sequence number (for ordering)
    int valid;          // 1 if frame is ready to display
} cached_frame_t;

// Fragment reassembly buffer (heap allocated data)
typedef struct {
    uint8_t *data;      // Heap-allocated reassembly buffer
    size_t received_size;
    size_t total_size;
    uint16_t seqnum;
    uint8_t frags_received;
    uint8_t total_frags;
    uint32_t frags_bitmap;  // Bitmap to track which fragments received (up to 32)
    int in_progress;    // 1 if currently receiving fragments
} fragment_buffer_t;

// Circular frame cache for jitter buffering
typedef struct {
    cached_frame_t frames[CACHE_SIZE];
    int write_idx;      // Next position to write
    int read_idx;       // Next position to read
    int count;          // Number of buffered frames
    int buffering;      // 1 if still filling initial buffer
    pthread_mutex_t mutex;
} frame_cache_t;

typedef struct {
    int rtp_socket_fd;
    pthread_t listen_thread_id;
    int stop_thread;

    // Fragment reassembly
    fragment_buffer_t frag_buf;

    // Frame cache for jitter buffering
    frame_cache_t cache;

    // Statistics
    rtp_stats_t stats;
    pthread_mutex_t stats_mutex;
} rtp_client_t;

int rtp_client_open_port(rtp_client_t *rtp, int port);

int rtp_client_start_listener(rtp_client_t *rtp);

// Get a frame from the cache (returns 0 if no frame available yet)
size_t rtp_client_get_frame(rtp_client_t *rtp, uint8_t *out_buffer);

// Get current statistics (thread-safe copy)
void rtp_client_get_stats(rtp_client_t *rtp, rtp_stats_t *out_stats);

// Get cache fill level (0-100%)
int rtp_client_get_buffer_level(rtp_client_t *rtp);

// Check if still in initial buffering phase
int rtp_client_is_buffering(rtp_client_t *rtp);

void rtp_client_stop_listener(rtp_client_t *rtp);

#endif // RTP_CLIENT_H
