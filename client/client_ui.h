#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include "rtsp_client.h"
#include "rtp_client.h"
#include "raylib.h"

#define SCREEN_WIDTH 800.0
#define SCREEN_HEIGHT 600.0
#define TOOLBAR_HEIGHT 50.0
#define TIMER_HEIGHT 30.0
#define VIDEO_HEIGHT (SCREEN_HEIGHT - TOOLBAR_HEIGHT - TIMER_HEIGHT)

typedef struct {
    // Core components
    rtsp_client_t *client;
    rtp_client_t *rtp;

    // Command line args
    char server_ip[64];
    int server_port;
    int rtp_port;
    char video_file[256];

    // UI elements
    Rectangle video_rect;
    Rectangle timer_rect;
    Rectangle toolbar_rect;
    Rectangle setup_btn_rect;
    Rectangle play_btn_rect;
    Rectangle pause_btn_rect;
    Rectangle teardown_btn_rect;

    // Video rendering
    RenderTexture2D video_texture;    // main video display
    unsigned char *frame_data_buffer; // buffer to get_frame
    bool close_signal;                // signal to close main loop
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
