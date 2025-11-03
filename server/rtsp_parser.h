#ifndef RTSP_PARSER_H
#define RTSP_PARSER_H

#include "../common/protocol.h"

typedef struct {
    rtsp_method_t method;
    char filename[256];
    int cseq;
    int rtp_port;
    int session_id;
} rtsp_request_info_t;

void rtsp_parse_request(const char *request_str, rtsp_request_info_t *info);

#endif // RTSP_PARSER_H
