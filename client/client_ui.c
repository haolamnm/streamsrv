#include "../common/logger.h"
#include "client_ui.h"
#include "raylib.h"

#include <stdio.h>

static bool IsButtonClicked(Rectangle rect) {
    return (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
}

void client_ui_init(client_ui_t *ui, rtsp_client_t *client) {
    ui->client = client;

    InitWindow(SCREEN_WIDTH, SCREEN_WIDTH, "RTSP CLIENT");
    SetTargetFPS(30);

    ui->video_rect = (Rectangle){ 0, 0, SCREEN_WIDTH, VIDEO_HEIGHT };
    ui->timer_rect = (Rectangle){ 0, VIDEO_HEIGHT, SCREEN_WIDTH, TIMER_HEIGHT };
    ui->toolbar_rect = (Rectangle){ 0, VIDEO_HEIGHT + TIMER_HEIGHT, SCREEN_WIDTH, TOOLBAR_HEIGHT };

    float btn_width = SCREEN_WIDTH / 4.0f;
    ui->setup_btn_rect = (Rectangle){ 0 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };
    ui->play_btn_rect = (Rectangle){ 1 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };
    ui->pause_btn_rect = (Rectangle){ 2 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };
    ui->teardown_btn_rect = (Rectangle){ 3 * btn_width, ui->toolbar_rect.y, btn_width, TOOLBAR_HEIGHT };
}

static void client_ui_update(client_ui_t *ui) {
    if (IsButtonClicked(ui->setup_btn_rect)) {
        logger_log("setup button clicked");
        // TODO: Call rtsp_client_send_setup()
    }

    if (IsButtonClicked(ui->play_btn_rect)) {
        logger_log("play button clicked");
        // TODO: Call rtsp_client_send_play()
    }

    if (IsButtonClicked(ui->pause_btn_rect)) {
        logger_log("pause button clicked");
        // TODO: Call rtsp_client_send_pause()
    }

    if (IsButtonClicked(ui->teardown_btn_rect)) {
        logger_log("teardown button clicked");
        // TODO: Call rtsp_client_send_teardown()
    }

    // TODO: Update video texture
    // TODO: Update timer
}

static void client_ui_draw(client_ui_t *ui) {
    BeginDrawing();
    ClearBackground(BLACK);

    // Draw video area
    DrawRectangleLinesEx(ui->video_rect, 1, DARKGRAY);
    DrawText("VIDEO PLAYBACK AREA",
        ui->video_rect.x + 20,
        ui->video_rect.y + 20, 20, GRAY
    );

    // Draw timer
    DrawRectangleRec(ui->timer_rect, DARKGRAY);
    char timer_text[16];
    sprintf(timer_text, "00:00");
    DrawText(timer_text,
        ui->timer_rect.x + (ui->timer_rect.width / 2) - 20,
        ui->timer_rect.y + (TIMER_HEIGHT / 2) - 10, 20, WHITE
    );

    // Draw toolbar
    DrawRectangleRec(ui->toolbar_rect, GRAY);

    // Draw the 4 buttons with simple text labels
    DrawRectangleLinesEx(ui->setup_btn_rect, 1, BLACK);
    DrawText("SETUP", ui->setup_btn_rect.x + 45, ui->setup_btn_rect.y + 15, 20, BLACK);

    DrawRectangleLinesEx(ui->play_btn_rect, 1, BLACK);
    DrawText("PLAY", ui->play_btn_rect.x + 55, ui->play_btn_rect.y + 15, 20, BLACK);

    DrawRectangleLinesEx(ui->pause_btn_rect, 1, BLACK);
    DrawText("PAUSE", ui->pause_btn_rect.x + 50, ui->pause_btn_rect.y + 15, 20, BLACK);

    DrawRectangleLinesEx(ui->teardown_btn_rect, 1, BLACK);
    DrawText("TEARDOWN", ui->teardown_btn_rect.x + 35, ui->teardown_btn_rect.y + 15, 20, BLACK);

    // We'll also draw button states later (e.g., disabled/enabled)

    EndDrawing();
}

void client_ui_run(client_ui_t *ui) {
    // This is the main game loop from raylib
    while (!WindowShouldClose()) {
        client_ui_update(ui);
        client_ui_draw(ui);
    }
}

void client_ui_cleanup(client_ui_t *ui) {
    // TODO: Unload video texture

    // We must close the window.
    CloseWindow();
}
