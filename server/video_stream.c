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

// Get total number of frames by scanning entire file
// This is slow but only called once for validation
int video_stream_get_total_frames(video_stream_t *stream) {
    if (stream->file == NULL) {
        return -1;
    }
    
    // Save current position
    long current_pos = ftell(stream->file);
    int current_frame = stream->frame_num;
    
    // Seek to beginning
    fseek(stream->file, 0, SEEK_SET);
    int total_frames = 0;
    
    char frame_len_str[MAX_FRAME_LEN_DIGITS + 1];
    int header_len = 0;
    
    while (1) {
        header_len = 0;
        
        // Read frame header (length)
        while (header_len < MAX_FRAME_LEN_DIGITS) {
            int ch = fgetc(stream->file);
            if (ch == EOF) {
                break;
            }
            if (isdigit(ch)) {
                frame_len_str[header_len++] = (char)ch;
            } else {
                ungetc(ch, stream->file);
                break;
            }
        }
        
        if (header_len == 0) {
            break;
        }
        
        frame_len_str[header_len] = '\0';
        int frame_len = atoi(frame_len_str);
        
        if (frame_len <= 0) {
            break;
        }
        
        // Skip frame data
        if (fseek(stream->file, frame_len, SEEK_CUR) != 0) {
            break;
        }
        
        total_frames++;
    }
    
    // Restore position
    fseek(stream->file, current_pos, SEEK_SET);
    stream->frame_num = current_frame;
    
    return total_frames;
}

// Seek to a specific time in seconds (assumes 20 FPS)
int video_stream_seek_time(video_stream_t *stream, double time_seconds) {
    if (stream->file == NULL) {
        return -1;
    }
    
    // Clamp to non-negative
    if (time_seconds < 0) {
        time_seconds = 0;
    }
    
    // Assuming 20 FPS frame rate
    const double FPS = 20.0;
    int target_frame = (int)(time_seconds * FPS);
    
    // Seek to beginning and scan frames until target. Keep file pointer
    // positioned at the start of the target frame's length header so that
    // the next call to video_stream_next_frame() will read it normally.
    if (fseek(stream->file, 0, SEEK_SET) != 0) {
        logger_log("failed to seek to file start");
        return -1;
    }

    stream->frame_num = 0;

    char frame_len_str[MAX_FRAME_LEN_DIGITS + 1];

    for (int i = 0; i < target_frame; i++) {
        int header_len = 0;
        // Read frame header (digits)
        while (header_len < MAX_FRAME_LEN_DIGITS) {
            int ch = fgetc(stream->file);
            if (ch == EOF) {
                logger_log("reached EOF while seeking to frame %d (stopped at %d)", target_frame, i);
                return stream->frame_num; // stopped before target
            }
            if (isdigit(ch)) {
                frame_len_str[header_len++] = (char)ch;
            } else {
                ungetc(ch, stream->file);
                break;
            }
        }
        if (header_len == 0) {
            logger_log("invalid frame header while seeking at frame %d", i);
            return -1;
        }
        frame_len_str[header_len] = '\0';
        int frame_len = atoi(frame_len_str);
        if (frame_len <= 0) {
            logger_log("invalid frame length while seeking: %d", frame_len);
            return -1;
        }

        // Skip this frame's data
        if (fseek(stream->file, frame_len, SEEK_CUR) != 0) {
            logger_log("seek failed at frame %d", i);
            return -1;
        }

        stream->frame_num++;
    }

    // We're positioned at the first digit of the target frame's header.
    logger_log("seeked to frame %d (time %.2f seconds)", target_frame, time_seconds);
    return target_frame;
}

// Seek to a specific frame number (0-indexed)
int video_stream_seek_frame(video_stream_t *stream, int frame_number) {
    if (stream->file == NULL) {
        return -1;
    }
    
    // Clamp to non-negative
    if (frame_number < 0) {
        frame_number = 0;
    }
    
    // Seek to beginning and skip frames until we reach the requested frame.
    if (fseek(stream->file, 0, SEEK_SET) != 0) {
        logger_log("failed to seek to file start");
        return -1;
    }

    stream->frame_num = 0;

    char frame_len_str[MAX_FRAME_LEN_DIGITS + 1];

    for (int i = 0; i < frame_number; i++) {
        int header_len = 0;
        while (header_len < MAX_FRAME_LEN_DIGITS) {
            int ch = fgetc(stream->file);
            if (ch == EOF) {
                logger_log("reached EOF while seeking to frame %d (stopped at %d)", frame_number, i);
                return stream->frame_num; // stopped before target
            }
            if (isdigit(ch)) {
                frame_len_str[header_len++] = (char)ch;
            } else {
                ungetc(ch, stream->file);
                break;
            }
        }
        if (header_len == 0) {
            logger_log("invalid frame header while seeking at frame %d", i);
            return -1;
        }
        frame_len_str[header_len] = '\0';
        int frame_len = atoi(frame_len_str);
        if (frame_len <= 0) {
            logger_log("invalid frame length while seeking: %d", frame_len);
            return -1;
        }

        if (fseek(stream->file, frame_len, SEEK_CUR) != 0) {
            logger_log("seek failed at frame %d", i);
            return -1;
        }

        stream->frame_num++;
    }

    // Now positioned at the start of the requested frame header
    logger_log("seeked to frame %d", frame_number);
    return frame_number;
}
