#ifndef VIDEO_STREAM_H
#define VIDEO_STREAM_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    FILE *file;
    int frame_num;
} video_stream_t;

int video_stream_open(video_stream_t *stream, const char *filename);

// Read next frame from the file into the buffer
// Return the size of the frame, or 0 if EOF, or -1 on error
ssize_t video_stream_next_frame(video_stream_t *stream, uint8_t *buffer, size_t buffer_size);

void video_stream_close(video_stream_t *stream);

#endif // VIDEO_STREAM_H
