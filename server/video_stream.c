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
    stream->total_frames = 0;
    stream->file_size = 0;
    stream->avg_frame_size = 0.0;
    stream->is_raw_mjpeg = 0;
    
    // Get file size
    fseek(stream->file, 0, SEEK_END);
    stream->file_size = ftell(stream->file);
    fseek(stream->file, 0, SEEK_SET);
    
    logger_log("video file opened: %s, size: %ld bytes, file ptr: %p", filename, stream->file_size, (void*)stream->file);
    return 0;
}

ssize_t video_stream_next_frame(video_stream_t *stream, uint8_t *buffer, size_t buffer_size) {
    char frame_len_str[MAX_FRAME_LEN_DIGITS + 1];
    int header_len = 0;

    if (stream->file == NULL) {
        logger_log("video stream file is NULL!");
        return -1;
    }

    // Peek at first byte to detect format
    int first_byte = fgetc(stream->file);
    if (first_byte == EOF) {
        logger_log("end of video reached");
        return 0;
    }
    
    // Check if this looks like a JPEG header (0xFF = 255)
    if (first_byte == 0xFF) {
        // Raw MJPEG format: JPEG frames directly concatenated
        // Find next JPEG frame by scanning for FFD8FFxx pattern (JPEG SOI marker + next marker)
        buffer[0] = first_byte;
        size_t pos = 1;
        
        // Read rest of file looking for next JPEG start or EOL
        while (pos < buffer_size) {
            int ch = fgetc(stream->file);
            if (ch == EOF) {
                // End of file - return this frame
                stream->frame_num++;
                return pos;
            }
            
            buffer[pos] = ch;
            
            // Look for JPEG EOI marker (FFD9) followed by new JPEG SOI (FFD8)
            if (pos >= 2 && buffer[pos-1] == 0xFF && buffer[pos] == 0xD9) {
                // Found EOI marker, check if next is SOI
                int next1 = fgetc(stream->file);
                if (next1 == EOF) {
                    stream->frame_num++;
                    return pos + 1;
                }
                
                if (next1 == 0xFF) {
                    int next2 = fgetc(stream->file);
                    if (next2 == 0xD8) {
                        // Found next JPEG frame start - put them back and return this frame
                        ungetc(next2, stream->file);
                        ungetc(next1, stream->file);
                        stream->frame_num++;
                        return pos + 1;
                    } else {
                        // Not FFD8, continue reading
                        buffer[pos + 1] = next1;
                        buffer[pos + 2] = next2;
                        pos += 2;
                        continue;
                    }
                } else {
                    // Not 0xFF, continue reading
                    buffer[pos + 1] = next1;
                    pos++;
                    continue;
                }
            }
            
            pos++;
        }
        
        stream->frame_num++;
        return pos;
    } else if (isdigit(first_byte)) {
        // Format with length header: "NNNNN" + frame_data
        // first_byte is already the first digit
        frame_len_str[0] = first_byte;
        header_len = 1;
        
        // Read remaining ASCII digits
        while (header_len < MAX_FRAME_LEN_DIGITS) {
            int ch = fgetc(stream->file);
            if (ch == EOF) {
                break;
            }
            
            if (isdigit(ch)) {
                frame_len_str[header_len++] = (char)ch;
            } else {
                // Non-digit found - put it back
                ungetc(ch, stream->file);
                break;
            }
        }

        frame_len_str[header_len] = '\0';
        int frame_len = atoi(frame_len_str);
        
        if (frame_len <= 0) {
            logger_log("invalid frame length: %s", frame_len_str);
            return -1;
        }

        // Check if the frame will fit into the buffer
        if ((size_t)frame_len > buffer_size) {
            logger_log("frame too large: %d bytes (buffer: %zu)", frame_len, buffer_size);
            fseek(stream->file, frame_len, SEEK_CUR);
            return -1;
        }

        // Read frame data
        size_t bytes_read = fread(buffer, 1, frame_len, stream->file);
        if (bytes_read != (size_t)frame_len) {
            logger_log("incomplete frame read: got %zu, expected %d", bytes_read, frame_len);
            return -1;
        }
        stream->frame_num++;
        return frame_len;
    } else {
        logger_log("unknown frame format: first byte 0x%02X", first_byte);
        return -1;
    }
}

void video_stream_close(video_stream_t *stream) {
    if (stream->file) {
        fclose(stream->file);
        stream->file = NULL;
    }
}

// Get total number of frames by scanning entire file
// For raw MJPEG: scans backwards from end (faster than forward scan)
// This is slow but only called once for validation
int video_stream_get_total_frames(video_stream_t *stream) {
    if (stream->file == NULL) {
        return -1;
    }
    
    // If already cached, return it
    if (stream->total_frames > 0) {
        return stream->total_frames;
    }
    
    // Save current position
    long current_pos = ftell(stream->file);
    int current_frame = stream->frame_num;
    
    // Seek to beginning to detect format
    fseek(stream->file, 0, SEEK_SET);
    int total_frames = 0;
    
    // Detect format from first byte
    int first_byte = fgetc(stream->file);
    if (first_byte == EOF) {
        fseek(stream->file, current_pos, SEEK_SET);
        return 0;
    }
    
    if (first_byte == 0xFF) {
        // Raw MJPEG format - count FFD9 (EOI) markers from end (faster!)
        stream->is_raw_mjpeg = 1;
        
        // Seek to end of file
        fseek(stream->file, 0, SEEK_END);
        long file_size = ftell(stream->file);
        
        // Scan backwards from end, looking for FFD9 markers
        int prev_byte = -1;
        
        for (long pos = file_size - 1; pos >= 0; pos--) {
            fseek(stream->file, pos, SEEK_SET);
            int ch = fgetc(stream->file);
            if (ch == EOF) break;
            
            // Look for FFD9 (JPEG EOI marker) to count frames
            // When scanning backwards: if current is 0xD9 and next (which was previous) is 0xFF
            if (prev_byte == 0xD9 && ch == 0xFF) {
                total_frames++;
            }
            
            prev_byte = ch;
        }
        
        // Calculate average frame size for seeking
        if (total_frames > 0) {
            stream->avg_frame_size = (double)file_size / total_frames;
        }
        
        logger_log("video_stream_get_total_frames: detected %d frames (raw MJPEG), avg_frame_size=%.0f bytes", 
                   total_frames, stream->avg_frame_size);
    } else if (isdigit(first_byte)) {
        // Format with length header - must scan from beginning
        stream->is_raw_mjpeg = 0;
        char frame_len_str[MAX_FRAME_LEN_DIGITS + 1];
        frame_len_str[0] = first_byte;
        int header_len = 1;
        
        while (1) {
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
            header_len = 0;
        }
        
        logger_log("video_stream_get_total_frames: detected %d frames (header format)", total_frames);
    }
    
    // Cache results
    stream->total_frames = total_frames;
    
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
    
    // Just use frame-based seeking
    return video_stream_seek_frame(stream, target_frame);
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
    
    // Always seek from beginning for reliable positioning
    if (fseek(stream->file, 0, SEEK_SET) != 0) {
        logger_log("failed to seek to file start");
        return -1;
    }

    // Detect format from first byte
    int first_byte = fgetc(stream->file);
    if (first_byte == EOF) {
        return -1;
    }
    
    if (first_byte == 0xFF) {
        // Raw MJPEG format - scan for FFD8 (SOI) markers
        ungetc(first_byte, stream->file);
        int frames_scanned = 0;
        int prev_byte = -1;
        
        while (1) {
            int ch = fgetc(stream->file);
            if (ch == EOF) {
                logger_log("reached EOF while seeking to frame %d (stopped at %d)", frame_number, frames_scanned);
                stream->frame_num = frames_scanned;
                return frames_scanned;
            }
            
            // Look for FFD8 (JPEG SOI marker)
            if (prev_byte == 0xFF && ch == 0xD8) {
                if (frames_scanned == frame_number) {
                    // Found target frame - back up to read full frame
                    fseek(stream->file, -2, SEEK_CUR);
                    stream->frame_num = frame_number;
                    logger_log("seeked to frame %d (raw MJPEG)", frame_number);
                    return frame_number;
                }
                frames_scanned++;
            }
            
            prev_byte = ch;
        }
    } else if (isdigit(first_byte)) {
        // Format with length header
        char frame_len_str[MAX_FRAME_LEN_DIGITS + 1];
        frame_len_str[0] = first_byte;
        int header_len = 1;
        
        for (int i = 0; i < frame_number; i++) {
            // Read frame header
            while (header_len < MAX_FRAME_LEN_DIGITS) {
                int ch = fgetc(stream->file);
                if (ch == EOF) {
                    logger_log("reached EOF while seeking to frame %d (stopped at %d)", frame_number, i);
                    return stream->frame_num;
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

            // Skip frame data
            if (fseek(stream->file, frame_len, SEEK_CUR) != 0) {
                logger_log("seek failed at frame %d", i);
                return -1;
            }

            stream->frame_num++;
            header_len = 0;
        }

        logger_log("seeked to frame %d (header format)", frame_number);
        return frame_number;
    } else {
        logger_log("unknown video format when seeking");
        return -1;
    }
}
