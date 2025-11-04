#include "../common/logger.h"
#include "client_ui.h"
#include "raylib.h"
#include "rtp_client.h"
#include "rtsp_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool IsButtonClicked(Rectangle rect) {
    return (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
}

void client_ui_init(
    client_ui_t *ui,
    rtsp_client_t *client,
    rtp_client_t *rtp,
    const char *server_ip,
    int server_port,
    int rtp_port,
    const char *video_file
) {
    ui->client = client;
    ui->rtp = rtp;
    ui->close_signal = false;

    // Store args for SETUP button
    strncpy(ui->server_ip, server_ip, sizeof(ui->server_ip) - 1);
    ui->server_port = server_port;
    ui->rtp_port = rtp_port;
    strncpy(ui->video_file, video_file, sizeof(ui->video_file) - 1);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "RTSP CLIENT");
    SetTargetFPS(30);

    ui->video_rect = (Rectangle){ 0, 0, SCREEN_WIDTH, VIDEO_HEIGHT };
    ui->timer_rect = (Rectangle){ 0, VIDEO_HEIGHT, SCREEN_WIDTH, TIMER_HEIGHT };
    ui->toolbar_rect = (Rectangle){ 0, VIDEO_HEIGHT + TIMER_HEIGHT, SCREEN_WIDTH, TOOLBAR_HEIGHT };

    float btn_width = SCREEN_WIDTH / 4.0f;
    ui->setup_btn_rect = (Rectangle){ 0 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };
    ui->play_btn_rect = (Rectangle){ 1 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };
    ui->pause_btn_rect = (Rectangle){ 2 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };
    ui->teardown_btn_rect = (Rectangle){ 3 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };

    ui->video_texture = LoadRenderTexture(SCREEN_WIDTH, VIDEO_HEIGHT);
    ui->frame_data_buffer = (unsigned char *)malloc(FRAME_BUFFER_SIZE);
}

static void client_ui_update_video(client_ui_t *ui) {
    size_t frame_size = rtp_client_get_frame(ui->rtp, ui->frame_data_buffer);

    if (frame_size > 0) {
        // Got a new frame
        Image img = LoadImageFromMemory(".jpg", ui->frame_data_buffer, frame_size);

        if (img.data != NULL) {
            Texture2D tex = LoadTextureFromImage(img); // convert img to texture

            BeginTextureMode(ui->video_texture);
            ClearBackground(BLACK);
            DrawTexturePro(tex,
                (Rectangle){ 0, 0, tex.width, tex.height },
                ui->video_rect, (Vector2){ 0, 0 }, 0, WHITE
            );
            EndTextureMode();

            UnloadTexture(tex);
            UnloadImage(img);
        } else {
            logger_log("failed to load image from memory");
        }
    }
}

static void client_ui_update_logic(client_ui_t *ui) {
    if (WindowShouldClose()) {
        ui->close_signal = true;
    }

    // Safely get the current state
    pthread_mutex_lock(&ui->client->state_mutex);
    client_state_t current_state = ui->client->state;
    pthread_mutex_unlock(&ui->client->state_mutex);

    if (IsButtonClicked(ui->setup_btn_rect) && current_state == STATE_INIT) {
        logger_log("setup button clicked");
        if (rtsp_client_connect(ui->client, ui->server_ip, ui->server_port, ui->video_file, ui->rtp_port) == 0) {
            if (rtp_client_open_port(ui->rtp, ui->rtp_port) == 0) {
                rtsp_client_send_setup(ui->client);
                rtsp_client_start_reply_listener(ui->client);
                rtp_client_start_listener(ui->rtp);
            }
        }
    }

    if (IsButtonClicked(ui->play_btn_rect) && current_state == STATE_READY) {
        logger_log("play button clicked");
        rtsp_client_send_play(ui->client);
        }

    if (IsButtonClicked(ui->pause_btn_rect) && current_state == STATE_PLAYING) {
        logger_log("pause button clicked");
        rtsp_client_send_pause(ui->client);
    }

    if (IsButtonClicked(ui->teardown_btn_rect) && current_state != STATE_INIT) {
        logger_log("teardown button clicked");
        ui->close_signal = true;
    }

    if (current_state == STATE_PLAYING) {
        client_ui_update_video(ui);
    }
}

static void client_ui_draw(client_ui_t *ui) {
    BeginDrawing();
    ClearBackground(BLACK);

    // Draw video
    // Flip it vertically because raylib's textures are loaded upside-down by default
    DrawTextureRec(ui->video_texture.texture,
        (Rectangle){ 0, 0, ui->video_texture.texture.width, -ui->video_texture.texture.height },
        (Vector2){ 0, 0 }, WHITE
    );

    // Draw timer
    DrawRectangleRec(ui->timer_rect, DARKGRAY);
    char timer_text[16];
    sprintf(timer_text, "00:00");
    DrawText(timer_text,
        ui->timer_rect.x + (ui->timer_rect.width / 2) - 20,
        ui->timer_rect.y + (TIMER_HEIGHT / 2) - 10, 20, WHITE
    );

    // Draw video area
    // DrawRectangleLinesEx(ui->video_rect, 1, DARKGRAY);
    // DrawText("VIDEO PLAYBACK AREA",
    //     ui->video_rect.x + 20,
    //     ui->video_rect.y + 20, 20, GRAY
    // );

    // Draw toolbar
    DrawRectangleRec(ui->toolbar_rect, GRAY);

    // Get the state to draw buttons correctly
    pthread_mutex_lock(&ui->client->state_mutex);
    client_state_t current_state = ui->client->state;
    pthread_mutex_unlock(&ui->client->state_mutex);

    // Disable buttons by drawing them a bit darker
    // if they are not clickable in the current state
    Color setup_col = (current_state == STATE_INIT) ? BLACK : DARKGRAY;
    Color play_col = (current_state == STATE_READY) ? BLACK : DARKGRAY;
    Color pause_col = (current_state == STATE_PLAYING) ? BLACK : DARKGRAY;
    Color teardown_col = (current_state != STATE_INIT) ? BLACK : DARKGRAY;

    // Draw the 4 buttons with simple text labels
    DrawRectangleLinesEx(ui->setup_btn_rect, 1, BLACK);
    DrawText("SETUP", ui->setup_btn_rect.x + 45, ui->setup_btn_rect.y + 15, 20, setup_col);

    DrawRectangleLinesEx(ui->play_btn_rect, 1, BLACK);
    DrawText("PLAY", ui->play_btn_rect.x + 55, ui->play_btn_rect.y + 15, 20, play_col);

    DrawRectangleLinesEx(ui->pause_btn_rect, 1, BLACK);
    DrawText("PAUSE", ui->pause_btn_rect.x + 50, ui->pause_btn_rect.y + 15, 20, pause_col);

    DrawRectangleLinesEx(ui->teardown_btn_rect, 1, BLACK);
    DrawText("TEARDOWN", ui->teardown_btn_rect.x + 35, ui->teardown_btn_rect.y + 15, 20, teardown_col);

    EndDrawing();
}

void client_ui_run(client_ui_t *ui) {
    // This is the main game loop from raylib
    while (ui->close_signal == 0) {
        client_ui_update_logic(ui);
        client_ui_draw(ui);
    }
}

void client_ui_cleanup(client_ui_t *ui) {
    logger_log("ui cleaning up...");

    // Send TEARDOWn if we are connected
    if (ui->client->state != STATE_INIT) {
        rtsp_client_send_teardown(ui->client);
    }

    // Stop networkd threads
    rtp_client_stop_listener(ui->rtp);
    rtsp_client_disconnect(ui->client);

    // Free raylib resources
    UnloadRenderTexture(ui->video_texture);
    free(ui->frame_data_buffer);

    CloseWindow();
}
