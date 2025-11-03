#include "rtsp_parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

static void parse_request_line(const char *line, rtsp_request_info_t *info) {
    char method_str[32];
    char url[256];
    char version[32];

    // sscanf is a simple way to parse a fixed format like this
    if (sscanf(line, "%31s %255s %31s", method_str, url, version) != 3) {
        return;
    }

    // Since we only care about the filename, take the part after final "/"
    char *filename = strrchr(url, '/');
    if (filename != NULL) {
        filename++; // move pas the last "/"
    } else {
        filename = url; // no "/" at all
    }
    strncpy(info->filename, filename, sizeof(info->filename) - 1);
    info->filename[sizeof(info->filename) - 1] = '\0';

    // Convert method in string into enum
    if (strcmp(method_str, "SETUP") == 0) {
        info->method = METHOD_SETUP;
    } else if (strcmp(method_str, "PLAY") == 0) {
        info->method = METHOD_PLAY;
    } else if (strcmp(method_str, "PAUSE") == 0) {
        info->method = METHOD_PAUSE;
    } else if (strcmp(method_str, "TEARDOWN") == 0) {
        info->method = METHOD_TEARDOWN;
    }
}

static void parse_header_line(const char *line, rtsp_request_info_t *info) {
    char header_name[64];
    char header_value[256];

    // Look for Name: Value format
    if (sscanf(line, "%63[^:]: %255[^\r\n]", header_name, header_value) != 2) {
        return;
    }

    if (strcasecmp(header_name, "CSeq") == 0) {
        info->cseq = atoi(header_value);
    } else if (strcasecmp(header_name, "Session") == 0) {
        info->session_id = atoi(header_value);
    } else if (strcasecmp(header_name, "Transport") == 0) {
        char *port_str = strstr(header_value, "client_port=");
        if (port_str != NULL) {
            port_str += 12; // move past "client_port="
            info->rtp_port = atoi(port_str);
        }
    }
}

void rtsp_parse_request(const char *request_str, rtsp_request_info_t *info) {
    memset(info, 0, sizeof(rtsp_request_info_t));
    info->method = METHOD_UNKNOWN;

    // Need a mutable copy of request_str because strtok_r will modify it in-place
    char *buffer = strdup(request_str);
    if (buffer == NULL) {
        return;
    }

    char *save_ptr;
    char *line = strtok_r(buffer, "\n", &save_ptr);

    if (line == NULL) {
        free(buffer);
        return;
    }

    // The first line is a special request line
    parse_request_line(line, info);

    // All subsequent lines are headers
    while((line = strtok_r(NULL, "\n", &save_ptr)) != NULL) {
        if (strlen(line) <= 1) {
            break; // an empty line signal the end of headers
        }
        parse_header_line(line, info);
    }

    free(buffer);
}
