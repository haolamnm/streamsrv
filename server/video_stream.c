#include "video_stream.h"
#include "../common/logger.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum digits for frame length (supports up to 999999 bytes = ~1MB per frame)
#define MAX_FRAME_LEN_DIGITS 10

int video_stream_open(video_stream_t *stream, const char *filename) {
    stream->file = fopen(filename, "rb");
    if (stream->file == NULL) {
        logger_log("failed to open video file: %s", filename);
        return -1;
    }
    stream->frame_num = 0;
    logger_log("video file opened: %s, file ptr: %p", filename, (void*)stream->file);
    return 0;
}

ssize_t video_stream_next_frame(video_stream_t *stream, uint8_t *buffer, size_t buffer_size) {
    char frame_len_str[MAX_FRAME_LEN_DIGITS + 1];
    int header_len = 0;

    if (stream->file == NULL) {
        logger_log("video stream file is NULL!");
        return -1;
    }

    // Read ASCII digits until we hit a non-digit (FF D8 starts with 0xFF)
    // This handles variable-length headers (5, 6, or more digits)
    while (header_len < MAX_FRAME_LEN_DIGITS) {
        int ch = fgetc(stream->file);
        if (ch == EOF) {
            if (header_len == 0) {
                logger_log("end of video reached");
                return 0;
            }
            break;
        }
        
        if (isdigit(ch)) {
            frame_len_str[header_len++] = (char)ch;
        } else {
            // Non-digit found - put it back and stop reading header
            ungetc(ch, stream->file);
            break;
        }
    }

    if (header_len == 0) {
        logger_log("invalid frame header - no digits found");
        return -1;
    }

    frame_len_str[header_len] = '\0';
    int frame_len = atoi(frame_len_str);
    
    if (frame_len <= 0) {
        logger_log("invalid frame length: %s", frame_len_str);
        return -1;
    }

    // Check if the frame will fit into the buffer to prevent buffer overflow
    if ((size_t)frame_len > buffer_size) {
        logger_log("frame too large: %d bytes (buffer: %zu)", frame_len, buffer_size);
        fseek(stream->file, frame_len, SEEK_CUR); // skip the frame
        return -1;
    }

    // Read frame data into the buffer
    size_t bytes_read = fread(buffer, 1, frame_len, stream->file);
    if (bytes_read != (size_t)frame_len) {
        logger_log("incomplete frame read: got %zu, expected %d", bytes_read, frame_len);
        return -1;
    }
    stream->frame_num++;
    return frame_len;
}

void video_stream_close(video_stream_t *stream) {
    if (stream->file) {
        fclose(stream->file);
        stream->file = NULL;
    }
}
