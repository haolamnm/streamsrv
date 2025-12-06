#include "../common/logger.h"
#include "../common/rtp_packet.h"
#include "../common/rtp_fragment.h"
#include "../common/protocol.h"
#include "rtp_client.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>

// Max single packet: RTP header + fragment header + MTU payload
#define RTP_RECV_BUFFER_SIZE (RTP_HEADER_SIZE + RTP_FRAG_HEADER_SIZE + RTP_MTU_PAYLOAD + 64)

// Minimum frames to buffer before starting playback
#define MIN_BUFFER_FRAMES 3

// Add a completed frame to the cache
static void cache_add_frame(rtp_client_t *rtp, const uint8_t *data, size_t size, uint16_t seqnum) {
    pthread_mutex_lock(&rtp->cache.mutex);
    
    // If cache is full, drop the OLDEST frame to make room for new one
    if (rtp->cache.count >= CACHE_SIZE) {
        // Advance read index to drop oldest frame
        rtp->cache.frames[rtp->cache.read_idx].valid = 0;
        rtp->cache.read_idx = (rtp->cache.read_idx + 1) % CACHE_SIZE;
        rtp->cache.count--;
        
        pthread_mutex_lock(&rtp->stats_mutex);
        rtp->stats.frames_dropped++;
        pthread_mutex_unlock(&rtp->stats_mutex);
    }
    
    cached_frame_t *frame = &rtp->cache.frames[rtp->cache.write_idx];
    
    if (size <= FRAME_BUFFER_SIZE) {
        memcpy(frame->data, data, size);
        frame->size = size;
        frame->seqnum = seqnum;
        frame->valid = 1;
        
        rtp->cache.write_idx = (rtp->cache.write_idx + 1) % CACHE_SIZE;
        rtp->cache.count++;
        
        // Check if we have enough frames to start playback
        if (rtp->cache.buffering && rtp->cache.count >= MIN_BUFFER_FRAMES) {
            rtp->cache.buffering = 0;
            logger_log("buffering complete, starting playback (cached %d frames)", rtp->cache.count);
        }
    }
    
    pthread_mutex_unlock(&rtp->cache.mutex);
}

// Process a fragment and reassemble frames
static void process_fragment(rtp_client_t *rtp, const uint8_t *payload, size_t payload_size, uint16_t seqnum) {
    if (payload_size < RTP_FRAG_HEADER_SIZE) {
        return;
    }
    
    rtp_frag_header_t frag_header;
    rtp_frag_decode(payload, &frag_header);
    
    const uint8_t *frag_data = payload + RTP_FRAG_HEADER_SIZE;
    size_t frag_size = payload_size - RTP_FRAG_HEADER_SIZE;
    
    fragment_buffer_t *buf = &rtp->frag_buf;
    
    if (rtp_frag_is_first(&frag_header)) {
        // Start of new frame - abandon any incomplete previous frame
        if (buf->in_progress && buf->seqnum != seqnum) {
            // Previous frame was incomplete, drop it
            pthread_mutex_lock(&rtp->stats_mutex);
            rtp->stats.frames_dropped++;
            pthread_mutex_unlock(&rtp->stats_mutex);
        }
        buf->seqnum = seqnum;
        buf->total_size = frag_header.total_size;
        buf->received_size = 0;
        buf->frags_received = 0;
        buf->frags_bitmap = 0;  // Reset bitmap
        buf->total_frags = rtp_calc_fragments(frag_header.total_size);
        buf->in_progress = 1;
    }
    
    // All fragments of same frame share the same seqnum
    if (!buf->in_progress || buf->seqnum != seqnum) {
        // No frame in progress or fragment from different frame, skip
        return;
    }
    
    // Check if we already received this fragment (duplicate)
    if (frag_header.frag_index < 32 && (buf->frags_bitmap & (1u << frag_header.frag_index))) {
        return;  // Already have this fragment
    }
    
    // Copy fragment data
    size_t offset = frag_header.frag_index * RTP_MTU_PAYLOAD;
    if (offset + frag_size <= FRAME_BUFFER_SIZE) {
        memcpy(buf->data + offset, frag_data, frag_size);
        buf->received_size += frag_size;
        buf->frags_received++;
        if (frag_header.frag_index < 32) {
            buf->frags_bitmap |= (1u << frag_header.frag_index);
        }
    }
    
    // Check if frame is complete
    if (buf->frags_received == buf->total_frags) {
        // Frame complete, add to cache
        cache_add_frame(rtp, buf->data, buf->received_size, seqnum);
        buf->in_progress = 0;
        
        pthread_mutex_lock(&rtp->stats_mutex);
        rtp->stats.frames_received++;
        pthread_mutex_unlock(&rtp->stats_mutex);
    }
}

// Process a non-fragmented frame (legacy/small frames)
static void process_single_frame(rtp_client_t *rtp, const uint8_t *payload, size_t payload_size, uint16_t seqnum) {
    cache_add_frame(rtp, payload, payload_size, seqnum);
    
    pthread_mutex_lock(&rtp->stats_mutex);
    rtp->stats.frames_received++;
    pthread_mutex_unlock(&rtp->stats_mutex);
}

static void *rtp_listen_thread(void *arg) {
    rtp_client_t *rtp = (rtp_client_t *)arg;
    uint8_t recv_buffer[RTP_RECV_BUFFER_SIZE];

    logger_log("rtp listen thread started (with caching)");

    while (rtp->stop_thread == 0) {
        ssize_t bytes_read = recvfrom(
            rtp->rtp_socket_fd, recv_buffer, RTP_RECV_BUFFER_SIZE, 0, NULL, NULL
        );
        if (bytes_read <= 0) {
            if (rtp->stop_thread != 0) {
                break;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                logger_log("rtp recv error: %s", strerror(errno));
            }
            continue;
        }

        // Received a packet, now decode it
        rtp_header_t header;
        uint8_t *payload;
        size_t payload_size = rtp_packet_decode(&header, &payload, recv_buffer, bytes_read);

        if (payload_size == 0) {
            continue;
        }

        // Update statistics
        pthread_mutex_lock(&rtp->stats_mutex);
        rtp->stats.packets_received++;
        
        if (!rtp->stats.first_packet) {
            rtp->stats.first_packet = 1;
            rtp->stats.last_seqnum = header.seqnum;
        } else {
            // Check for packet loss (sequence number gap)
            uint16_t expected = rtp->stats.last_seqnum + 1;
            if (header.seqnum != expected && header.seqnum > expected) {
                uint16_t lost = header.seqnum - expected;
                rtp->stats.packets_lost += lost;
            }
            rtp->stats.last_seqnum = header.seqnum;
        }
        pthread_mutex_unlock(&rtp->stats_mutex);

        // Check if this is a fragmented packet
        // JPEG files always start with 0xFF 0xD8 (SOI marker)
        // Fragmented packets have fragment header first
        if (payload_size >= 2 && payload[0] == 0xFF && payload[1] == 0xD8) {
            // Raw JPEG frame (non-fragmented)
            process_single_frame(rtp, payload, payload_size, header.seqnum);
        } else if (payload_size >= RTP_FRAG_HEADER_SIZE) {
            // Fragmented packet
            process_fragment(rtp, payload, payload_size, header.seqnum);
        }
    }
    
    logger_log("rtp listen thread stopping");
    return NULL;
}

int rtp_client_open_port(rtp_client_t *rtp, int port) {
    // Initialize cache
    memset(&rtp->cache, 0, sizeof(frame_cache_t));
    pthread_mutex_init(&rtp->cache.mutex, NULL);
    rtp->cache.buffering = 1;  // Start in buffering mode
    
    // Initialize fragment buffer
    memset(&rtp->frag_buf, 0, sizeof(fragment_buffer_t));
    
    // Initialize statistics
    memset(&rtp->stats, 0, sizeof(rtp_stats_t));
    pthread_mutex_init(&rtp->stats_mutex, NULL);
    
    rtp->stop_thread = 1;
    rtp->rtp_socket_fd = -1;

    // Create UDP socket
    rtp->rtp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtp->rtp_socket_fd < 0) {
        logger_log("error creating rtp socket: %s", strerror(errno));
        return -1;
    }

    // Increase socket receive buffer size for HD frames
    int rcvbuf_size = 512 * 1024; // 512KB
    if (setsockopt(rtp->rtp_socket_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size)) < 0) {
        logger_log("warning: could not set socket receive buffer size: %s", strerror(errno));
    }

    // Set a short timeout on socket
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
    
    logger_log("rtp port opened and bound to %d (with %d-frame cache)", port, CACHE_SIZE);
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
    
    pthread_mutex_lock(&rtp->cache.mutex);
    
    // Don't return frames while still in initial buffering
    if (rtp->cache.buffering) {
        pthread_mutex_unlock(&rtp->cache.mutex);
        return 0;
    }
    
    // Check if there's a frame to read
    if (rtp->cache.count > 0) {
        cached_frame_t *frame = &rtp->cache.frames[rtp->cache.read_idx];
        
        if (frame->valid) {
            memcpy(out_buffer, frame->data, frame->size);
            frame_size = frame->size;
            frame->valid = 0;
            
            rtp->cache.read_idx = (rtp->cache.read_idx + 1) % CACHE_SIZE;
            rtp->cache.count--;
        }
    }
    // If cache is empty, just return 0 - UI will keep showing last frame
    // No re-buffering needed, frames will arrive soon
    
    pthread_mutex_unlock(&rtp->cache.mutex);
    
    return frame_size;
}

void rtp_client_get_stats(rtp_client_t *rtp, rtp_stats_t *out_stats) {
    pthread_mutex_lock(&rtp->stats_mutex);
    memcpy(out_stats, &rtp->stats, sizeof(rtp_stats_t));
    pthread_mutex_unlock(&rtp->stats_mutex);
}

int rtp_client_get_buffer_level(rtp_client_t *rtp) {
    pthread_mutex_lock(&rtp->cache.mutex);
    int level = (rtp->cache.count * 100) / CACHE_SIZE;
    pthread_mutex_unlock(&rtp->cache.mutex);
    return level;
}

int rtp_client_is_buffering(rtp_client_t *rtp) {
    pthread_mutex_lock(&rtp->cache.mutex);
    int buffering = rtp->cache.buffering;
    pthread_mutex_unlock(&rtp->cache.mutex);
    return buffering;
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
    pthread_mutex_destroy(&rtp->cache.mutex);
    pthread_mutex_destroy(&rtp->stats_mutex);
}
