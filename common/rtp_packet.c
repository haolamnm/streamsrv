#include "rtp_packet.h"
#include "protocol.h"

#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

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
) {
    // Check if the total packet will fit in the provided buffer to prevent
    // buffer overflow
    size_t packet_size = RTP_HEADER_SIZE + payload_size;
    if (buffer_size < packet_size) {
        return 0;
    }
    rtp_header_t *header = (rtp_header_t *)packet_buffer;
    header->version = version;
    header->padding = padding;
    header->extension = extension;
    header->cc = cc;
    header->marker = marker;
    header->pt = pt;
    header->seqnum = htons(seqnum);
    header->ssrc = htonl(ssrc);
    header->timestamp = htonl((uint32_t)time(NULL));

    // Copy the payload data into the buffer following the 12-byte header
    memcpy(packet_buffer + RTP_HEADER_SIZE, payload, payload_size);
    return packet_size;
}

size_t rtp_packet_decode(
    rtp_header_t *header,
    uint8_t **payload,
    const uint8_t *packet_buffer,
    size_t packet_size
) {
    // Can't decode a packet that's smaller than the header itself
    if (packet_size < RTP_HEADER_SIZE) {
        return 0;
    }

    // "Overlay" the header struct onto the incoming buffer
    const rtp_header_t *in_header = (const rtp_header_t *)packet_buffer;

    // Direct struct assignment might work, but this is safer and avoids
    // potential memory alignment issues
    header->version = in_header->version;
    header->padding = in_header->padding;
    header->extension = in_header->extension;
    header->cc = in_header->cc;
    header->marker = in_header->marker;
    header->pt = in_header->pt;
    header->seqnum = ntohs(in_header->seqnum);
    header->timestamp = ntohl(in_header->timestamp);
    header->ssrc = ntohl(in_header->ssrc);

    // This is efficient as it avoids a memory copy
    *payload = (uint8_t *)(packet_buffer + RTP_HEADER_SIZE);

    // The payload size is simply the total packet size minus the header size
    return packet_size - RTP_HEADER_SIZE;
}
