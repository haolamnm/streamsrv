#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include "rtsp_client.h"
#include "raylib.h"

#define SCREEN_WIDTH 800.0
#define SCREEN_HEIGHT 600.0
#define TOOLBAR_HEIGHT 50.0
#define TIMER_HEIGHT 30.0
#define VIDEO_HEIGHT (SCREEN_HEIGHT - TOOLBAR_HEIGHT - TIMER_HEIGHT)

typedef struct {
    rtsp_client_t *client;

    Rectangle video_rect;
    Rectangle timer_rect;
    Rectangle toolbar_rect;

    Rectangle setup_btn_rect;
    Rectangle play_btn_rect;
    Rectangle pause_btn_rect;
    Rectangle teardown_btn_rect;
} client_ui_t;

void client_ui_init(client_ui_t *ui, rtsp_client_t *client);

void client_ui_run(client_ui_t *ui);

void client_ui_cleanup(client_ui_t *ui);

#endif // CLIENT_UI_H
