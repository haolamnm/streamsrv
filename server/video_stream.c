#include "video_stream.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Assume the MJPEG file format: [5 bytes ASCII frame length][frame data]
#define FRAME_LEN_HEADER_SIZE 5

int video_stream_open(video_stream_t *stream, const char *filename) {
    stream->file = fopen(filename, "rb");
    if (stream->file == NULL) {
        return -1;
    }
    stream->frame_num = 0;
    return 0;
}

ssize_t video_stream_next_frame(video_stream_t *stream, uint8_t *buffer, size_t buffer_size) {
    char frame_len_str[FRAME_LEN_HEADER_SIZE + 1];

    if (fread(frame_len_str, 1, FRAME_LEN_HEADER_SIZE, stream->file) != FRAME_LEN_HEADER_SIZE) {
        // If we can't read the header, assume we've hit the EOF
        return 0;
    }
    frame_len_str[FRAME_LEN_HEADER_SIZE] = '\0';
    int frame_len = atoi(frame_len_str);

    // Check if the frame will fit into the buffer to prever buffer overflow
    if ((size_t)frame_len > buffer_size) {
        fseek(stream->file, frame_len, SEEK_CUR); // skip the frame
        return -1;
    }

    // Read frame data into the buffer
    if (fread(buffer, 1, frame_len, stream->file) != (size_t)frame_len) {
        // This block indicate corrupted file or unexpected EOF
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
