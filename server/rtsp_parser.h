#ifndef RTSP_PARSER_H
#define RTSP_PARSER_H

#include "../common/protocol.h"

typedef struct {
    rtsp_method_t method;
    char filename[256];
    int cseq;
    int rtp_port;
    int session_id;
    double seek_position;  // Position in seconds for PLAY with Range header
    int has_seek;          // Flag if seek_position is set
    int frame_number;      // Frame number for seek (custom X-Frame header)
    int has_frame_seek;    // Flag if frame_number is set
} rtsp_request_info_t;

void rtsp_parse_request(const char *request_str, rtsp_request_info_t *info);

#endif // RTSP_PARSER_H
