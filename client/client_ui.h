#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include "rtsp_client.h"
#include "rtp_client.h"
#include "raylib.h"

// Initial window size (will be resized when video loads)
#define INITIAL_VIDEO_WIDTH 640
#define INITIAL_VIDEO_HEIGHT 480
#define TOOLBAR_HEIGHT 60.0
#define TIMER_HEIGHT 40.0
#define STATS_HEIGHT 25.0  // New stats bar

// Modern color palette
#define UI_BG_COLOR (Color){ 30, 30, 30, 255 }
#define UI_TOOLBAR_COLOR (Color){ 45, 45, 45, 255 }
#define UI_TIMER_COLOR (Color){ 35, 35, 35, 255 }
#define UI_STATS_COLOR (Color){ 25, 25, 25, 255 }
#define UI_ACCENT_COLOR (Color){ 66, 133, 244, 255 }
#define UI_ACCENT_HOVER (Color){ 100, 160, 255, 255 }
#define UI_DANGER_COLOR (Color){ 234, 67, 53, 255 }
#define UI_DANGER_HOVER (Color){ 255, 100, 90, 255 }
#define UI_SUCCESS_COLOR (Color){ 52, 168, 83, 255 }
#define UI_WARNING_COLOR (Color){ 251, 188, 4, 255 }
#define UI_DISABLED_COLOR (Color){ 80, 80, 80, 255 }
#define UI_TEXT_COLOR (Color){ 255, 255, 255, 255 }
#define UI_TEXT_DIM_COLOR (Color){ 150, 150, 150, 255 }

typedef struct {
    // Core components
    rtsp_client_t *client;
    rtp_client_t *rtp;

    // Command line args
    char server_ip[64];
    int server_port;
    int rtp_port;
    char video_file[256];

    // Dynamic window size
    int video_width;
    int video_height;
    int screen_width;
    int screen_height;
    bool video_size_detected;

    // UI elements
    Rectangle video_rect;
    Rectangle timer_rect;
    Rectangle stats_rect;
    Rectangle toolbar_rect;
    Rectangle connect_btn_rect;
    Rectangle playpause_btn_rect;
    Rectangle stop_btn_rect;
    Rectangle seek_back_btn_rect;
    Rectangle seek_forward_btn_rect;

    // Video rendering
    RenderTexture2D video_texture;    // main video display
    unsigned char *frame_data_buffer; // buffer to get_frame
    bool close_signal;                // signal to close main loop

    // Timer tracking
    double play_start_time;
    double elapsed_time;
    bool timer_running;
    bool video_ended;              // Flag when video reaches EOF
    int frame_count;               // Number of frames received from server
    int frame_count_at_seek;       // Frame count when last seek was done
    int current_frame_number;      // Absolute frame number (0-500) being displayed

    // Frame rate control - throttle to match server's 20 FPS
    double last_frame_time;
    int consecutive_empty_frames;   // Counter for EOF detection

    // Auto-play after seek
    double seek_time_pending;      // Time when seek happened (for auto-play delay)
    bool auto_play_after_seek;     // Flag to trigger auto-play after delay

    // Statistics display
    rtp_stats_t last_stats;
    int last_buffer_level;
} client_ui_t;

void client_ui_init(
    client_ui_t *ui,
    rtsp_client_t *client,
    rtp_client_t *rtp,
    const char *server_ip,
    int server_port,
    int rtp_port,
    const char *video_file
);

void client_ui_run(client_ui_t *ui);

void client_ui_cleanup(client_ui_t *ui);

#endif // CLIENT_UI_H
