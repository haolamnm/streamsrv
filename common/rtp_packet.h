#ifndef RTP_PACKET_H
#define RTP_PACKET_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    // Byte 0
    uint8_t cc : 4;         // CSRC count
    uint8_t extension : 1;  // extension bit
    uint8_t padding : 1;    // padding bit
    uint8_t version : 2;    // version

    // Byte 1
    uint8_t pt : 7;         // payload type
    uint8_t marker : 1;     // marker bit

    // Byte 2-3
    uint16_t seqnum;        // sequence number

    // Byte 4-7
    uint32_t timestamp;     // timestamp

    // Byte 8-11
    uint32_t ssrc;          // sync source
} rtp_header_t;

size_t rtp_packet_encode(
    uint8_t *packet_buffer,
    size_t buffer_size,
    uint8_t version,
    uint8_t padding,
    uint8_t extension,
    uint8_t cc,
    uint16_t seqnum,
    uint8_t marker,
    uint8_t pt,
    uint32_t ssrc,
    const uint8_t *payload,
    size_t payload_size
);

size_t rtp_packet_decode(
    rtp_header_t *header,
    uint8_t **payload,
    const uint8_t *packet_buffer,
    size_t packet_size
);

#endif // RTP_PACKET_H
