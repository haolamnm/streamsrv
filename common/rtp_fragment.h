#ifndef RTP_FRAGMENT_H
#define RTP_FRAGMENT_H

#include <stdint.h>
#include <stddef.h>

// Max payload size per fragment (leaving room for headers)
// MTU is typically 1500, minus IP(20) + UDP(8) + RTP(12) + Fragment(4) = 1456
#define RTP_MTU_PAYLOAD 1400

// Fragment header format (4 bytes):
// Byte 0: Fragment flags (bit 7: first, bit 6: last, bits 0-5: reserved)
// Byte 1: Fragment index (0-255)
// Byte 2-3: Total frame size (high byte, low byte) - only in first fragment
#define FRAG_FLAG_FIRST 0x80
#define FRAG_FLAG_LAST  0x40

typedef struct {
    uint8_t flags;           // FRAG_FLAG_FIRST | FRAG_FLAG_LAST
    uint8_t frag_index;      // Fragment number (0, 1, 2, ...)
    uint16_t total_size;     // Total frame size (only meaningful in first fragment)
} rtp_frag_header_t;

#define RTP_FRAG_HEADER_SIZE 4

// Calculate number of fragments needed for a frame
static inline int rtp_calc_fragments(size_t frame_size) {
    return (frame_size + RTP_MTU_PAYLOAD - 1) / RTP_MTU_PAYLOAD;
}

// Encode a fragment header
static inline void rtp_frag_encode(uint8_t *buffer, int frag_index, int total_frags, size_t total_size) {
    uint8_t flags = 0;
    if (frag_index == 0) flags |= FRAG_FLAG_FIRST;
    if (frag_index == total_frags - 1) flags |= FRAG_FLAG_LAST;
    
    buffer[0] = flags;
    buffer[1] = (uint8_t)frag_index;
    buffer[2] = (uint8_t)((total_size >> 8) & 0xFF);
    buffer[3] = (uint8_t)(total_size & 0xFF);
}

// Decode a fragment header
static inline void rtp_frag_decode(const uint8_t *buffer, rtp_frag_header_t *header) {
    header->flags = buffer[0];
    header->frag_index = buffer[1];
    header->total_size = ((uint16_t)buffer[2] << 8) | buffer[3];
}

static inline int rtp_frag_is_first(const rtp_frag_header_t *header) {
    return (header->flags & FRAG_FLAG_FIRST) != 0;
}

static inline int rtp_frag_is_last(const rtp_frag_header_t *header) {
    return (header->flags & FRAG_FLAG_LAST) != 0;
}

#endif // RTP_FRAGMENT_H
