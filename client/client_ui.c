#include "../common/logger.h"
#include "client_ui.h"
#include "raylib.h"
#include "rtp_client.h"
#include "rtsp_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Adaptive frame timing constants (for buffer stabilization)
// Target buffer level: 75% (matching MIN_BUFFER_FRAMES)
#define BUFFER_TARGET_HIGH   80   // Above this: consume faster
#define BUFFER_TARGET_LOW    70   // Below this: consume slower

// Frame intervals in seconds (30 FPS base = 0.033s)
#define FRAME_INTERVAL_FAST   0.032  // ~31.25 FPS - drain buffer
#define FRAME_INTERVAL_NORMAL 0.033  // ~30.30 FPS - maintain buffer
#define FRAME_INTERVAL_SLOW   0.034  // ~29.41 FPS - fill buffer

// Helper function to check if mouse is hovering over a rectangle
static bool IsMouseOver(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect);
}

static bool IsButtonClicked(Rectangle rect) {
    return (IsMouseOver(rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
}

// Draw a rounded rectangle button with icon
static void DrawRoundedButton(Rectangle rect, Color baseColor, Color hoverColor, bool enabled, bool hovered) {
    Color color = enabled ? (hovered ? hoverColor : baseColor) : UI_DISABLED_COLOR;
    DrawRectangleRounded(rect, 0.3f, 8, color);
}

// Draw play icon (triangle pointing right)
static void DrawPlayIcon(float cx, float cy, float size, Color color) {
    Vector2 v1 = { cx - size * 0.4f, cy - size * 0.5f };
    Vector2 v2 = { cx - size * 0.4f, cy + size * 0.5f };
    Vector2 v3 = { cx + size * 0.5f, cy };
    DrawTriangle(v1, v2, v3, color);
}

// Draw pause icon (two vertical bars)
static void DrawPauseIcon(float cx, float cy, float size, Color color) {
    float barWidth = size * 0.25f;
    float barHeight = size * 0.7f;
    float gap = size * 0.15f;

    DrawRectangleRounded(
        (Rectangle){ cx - gap - barWidth, cy - barHeight / 2, barWidth, barHeight },
        0.2f, 4, color
    );
    DrawRectangleRounded(
        (Rectangle){ cx + gap, cy - barHeight / 2, barWidth, barHeight },
        0.2f, 4, color
    );
}

// Draw stop icon (square)
static void DrawStopIcon(float cx, float cy, float size, Color color) {
    float squareSize = size * 0.6f;
    DrawRectangleRounded(
        (Rectangle){ cx - squareSize / 2, cy - squareSize / 2, squareSize, squareSize },
        0.15f, 4, color
    );
}

// Draw connect icon (circle with dot)
static void DrawConnectIcon(float cx, float cy, float size, Color color) {
    DrawCircle(cx, cy, size * 0.35f, color);
    DrawRing((Vector2){ cx, cy }, size * 0.45f, size * 0.55f, 0, 360, 36, color);
}

// Draw seek back icon (rotate left/rewind)
static void DrawSeekBackIcon(float cx, float cy, float size, Color color) {
    // Bán kính từ tâm đến đỉnh
    float r = size * 0.4f; 
    
    // Tính toán 3 đỉnh của tam giác (Hướng sang trái - 180 độ)
    
    // Đỉnh 1: Mũi nhọn (bên trái)
    Vector2 v1 = { cx - r, cy };
    
    // Đỉnh 2: Góc dưới bên phải
    // x lệch về phải một nửa bán kính, y đi xuống
    Vector2 v2 = { cx + r * 0.5f, cy + r * 0.866f };
    
    // Đỉnh 3: Góc trên bên phải
    // x lệch về phải một nửa bán kính, y đi lên
    Vector2 v3 = { cx + r * 0.5f, cy - r * 0.866f };
    
    // Vẽ tam giác (thứ tự ngược chiều kim đồng hồ để hiển thị đúng trong Raylib)
    DrawTriangle(v1, v2, v3, color);
}

// Draw seek forward icon (rotate right/fast forward)
static void DrawSeekForwardIcon(float cx, float cy, float size, Color color) {
    // Bán kính từ tâm đến các đỉnh (khoảng 40% kích thước ô chứa)
    float r = size * 0.4f; 
    
    // Tính toán 3 đỉnh của tam giác (Hướng sang phải 0 độ)
    
    // Đỉnh 1: Mũi nhọn (bên phải)
    Vector2 v1 = { cx + r, cy };
    
    // Đỉnh 2: Góc dưới bên trái
    Vector2 v2 = { cx - r * 0.5f, cy + r * 0.866f };
    
    // Đỉnh 3: Góc trên bên trái
    Vector2 v3 = { cx - r * 0.5f, cy - r * 0.866f };
    
    // SỬA LỖI TẠI ĐÂY:
    // Đổi thứ tự v2 và v3 cho nhau: v1 -> v3 -> v2 (Ngược chiều kim đồng hồ)
    DrawTriangle(v1, v3, v2, color);
}
// Helper function to update UI layout based on current dimensions
static void client_ui_update_layout(client_ui_t *ui) {
    ui->video_rect = (Rectangle){ 0, 0, ui->screen_width, ui->video_height };
    ui->timer_rect = (Rectangle){ 0, ui->video_height, ui->screen_width, TIMER_HEIGHT };
    ui->stats_rect = (Rectangle){ 0, ui->video_height + TIMER_HEIGHT, ui->screen_width, STATS_HEIGHT };
    ui->toolbar_rect = (Rectangle){ 0, ui->video_height + TIMER_HEIGHT + STATS_HEIGHT, ui->screen_width, TOOLBAR_HEIGHT };

    // Button layout - centered with gaps
    float btn_size = 50.0f;
    float gap = 10.0f;
    float total_width = btn_size * 5 + gap * 4;
    float start_x = (ui->screen_width - total_width) / 2;
    float btn_y = ui->toolbar_rect.y + (TOOLBAR_HEIGHT - btn_size) / 2;

    ui->connect_btn_rect = (Rectangle){ start_x, btn_y, btn_size, btn_size };
    ui->seek_back_btn_rect = (Rectangle){ start_x + btn_size + gap, btn_y, btn_size, btn_size };
    ui->playpause_btn_rect = (Rectangle){ start_x + 2 * (btn_size + gap), btn_y, btn_size, btn_size };
    ui->seek_forward_btn_rect = (Rectangle){ start_x + 3 * (btn_size + gap), btn_y, btn_size, btn_size };
    ui->stop_btn_rect = (Rectangle){ start_x + 4 * (btn_size + gap), btn_y, btn_size, btn_size };
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

    // Timer initialization
    ui->play_start_time = 0;
    ui->elapsed_time = 0;
    ui->timer_running = false;
    ui->video_ended = false;
    ui->frame_count = 0;
    ui->frame_count_at_seek = 0;
    ui->current_frame_number = 0;
    ui->last_frame_time = 0;
    ui->consecutive_empty_frames = 0;

    // Auto-play after seek
    ui->seek_time_pending = 0;
    ui->auto_play_after_seek = false;

    // Statistics initialization
    memset(&ui->last_stats, 0, sizeof(rtp_stats_t));
    ui->last_buffer_level = 0;

    // Initialize with default size, will be updated when first frame arrives
    ui->video_width = INITIAL_VIDEO_WIDTH;
    ui->video_height = INITIAL_VIDEO_HEIGHT;
    ui->screen_width = INITIAL_VIDEO_WIDTH;
    ui->screen_height = INITIAL_VIDEO_HEIGHT + TIMER_HEIGHT + STATS_HEIGHT + TOOLBAR_HEIGHT;
    ui->video_size_detected = false;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(ui->screen_width, ui->screen_height, "RTSP Client");
    SetTargetFPS(30);

    client_ui_update_layout(ui);

    ui->video_texture = LoadRenderTexture(ui->video_width, ui->video_height);
    ui->frame_data_buffer = (unsigned char *)malloc(FRAME_BUFFER_SIZE);

    // Clear video texture to black initially
    BeginTextureMode(ui->video_texture);
    ClearBackground(BLACK);
    EndTextureMode();
}

static void client_ui_update_video(client_ui_t *ui, const uint8_t *frame_data, size_t frame_size) {
    if (frame_size <= 0) {
        return;
    }

    // Got a new frame
    Image img = LoadImageFromMemory(".jpg", frame_data, frame_size);

    if (img.data != NULL) {
        // Detect video resolution from first frame and resize window
        if (!ui->video_size_detected && img.width > 0 && img.height > 0) {
            ui->video_width = img.width;
            ui->video_height = img.height;
            ui->screen_width = img.width;
            ui->screen_height = img.height + (int)TIMER_HEIGHT + (int)STATS_HEIGHT + (int)TOOLBAR_HEIGHT;
            ui->video_size_detected = true;

            // Resize window to match video
            SetWindowSize(ui->screen_width, ui->screen_height);

            // Recreate video texture with correct size
            UnloadRenderTexture(ui->video_texture);
            ui->video_texture = LoadRenderTexture(ui->video_width, ui->video_height);

            // Update layout
            client_ui_update_layout(ui);

            logger_log("video resolution detected: %dx%d", img.width, img.height);
        }

        Texture2D tex = LoadTextureFromImage(img); // convert img to texture

        BeginTextureMode(ui->video_texture);
        ClearBackground(BLACK);
        // Draw at native resolution (1:1)
        DrawTexture(tex, 0, 0, WHITE);
        EndTextureMode();

        UnloadTexture(tex);
        UnloadImage(img);
    } else {
        logger_log("failed to load image from memory");
    }
}

static void client_ui_update_logic(client_ui_t *ui) {
    if (WindowShouldClose()) {
        ui->close_signal = true;
    }

    // Handle window resizing
    if (IsWindowResized()) {
        ui->screen_width = GetScreenWidth();
        ui->screen_height = GetScreenHeight();
        ui->video_height = ui->screen_height - (int)TIMER_HEIGHT - (int)STATS_HEIGHT - (int)TOOLBAR_HEIGHT;
        client_ui_update_layout(ui);
    }

    // Safely get the current state
    pthread_mutex_lock(&ui->client->state_mutex);
    client_state_t current_state = ui->client->state;
    pthread_mutex_unlock(&ui->client->state_mutex);

    // Update timer based on absolute frame number (frame / FPS)
    // Timer reflects the absolute position in the video regardless of seeks
    if (ui->timer_running) {
        const double FPS = 20.0;
        ui->elapsed_time = ((double)ui->current_frame_number) / FPS;
    }

    // Update statistics periodically
    if (current_state != STATE_INIT) {
        rtp_client_get_stats(ui->rtp, &ui->last_stats);
        ui->last_buffer_level = rtp_client_get_buffer_level(ui->rtp);
    }

    // Connect button - only works in INIT state
    if (IsButtonClicked(ui->connect_btn_rect) && current_state == STATE_INIT) {
        logger_log("connect button clicked");
        if (rtsp_client_connect(ui->client, ui->server_ip, ui->server_port, ui->video_file, ui->rtp_port) == 0) {
            if (rtp_client_open_port(ui->rtp, ui->rtp_port) == 0) {
                rtsp_client_send_setup(ui->client);
                rtsp_client_start_reply_listener(ui->client);
                rtp_client_start_listener(ui->rtp);
            }
        }
    }

    // Play/Pause toggle button
    if (IsButtonClicked(ui->playpause_btn_rect)) {
        if (current_state == STATE_READY) {
            logger_log("play button clicked");
            rtsp_client_send_play(ui->client);
            ui->play_start_time = GetTime();
            // Anchor the seek/frame counters to the current absolute frame
            ui->frame_count = 0;
            ui->frame_count_at_seek = ui->current_frame_number;  // Reset seek anchor when play starts
            ui->last_frame_time = GetTime();  // Reset frame timing for smooth start
            ui->timer_running = true;
        } else if (current_state == STATE_PLAYING) {
            logger_log("pause button clicked");
            rtsp_client_send_pause(ui->client);
            ui->timer_running = false;
        }
    }

    // Seek back button - go back 3 seconds
    if (IsButtonClicked(ui->seek_back_btn_rect) && current_state != STATE_INIT) {
        // Compute target frame relative to the absolute current frame
        const double FPS = 20.0;
        int current_frame = ui->current_frame_number;
        int target_frame = current_frame - (int)(3.0 * FPS);
        if (target_frame < 0) target_frame = 0;
        double new_time = (double)target_frame / FPS;
        logger_log("seek back button clicked - seeking to frame %d (%.1f seconds)", target_frame, new_time);
        rtsp_client_send_seek_frame(ui->client, target_frame);
        
        // Reset frame counter and elapsed_time after seek
        // Server will send frames starting from target_frame
        ui->frame_count = 0;                 // Reset frame counter (server starts from target)
        ui->frame_count_at_seek = target_frame; // Anchor absolute frame number for upcoming frames
        ui->current_frame_number = target_frame;  // Set absolute frame number immediately
        ui->elapsed_time = 0;          // Reset timer to 0
        
        ui->video_ended = false;  // Reset EOF flag after seek (don't stop during seek)
        ui->consecutive_empty_frames = 0;
        ui->last_frame_time = GetTime();
        // Clear cache to discard old frames and start buffering from new position
        rtp_client_clear_cache(ui->rtp);
        
        // Set auto-play after seek with 0.2s delay
        ui->seek_time_pending = GetTime();
        ui->auto_play_after_seek = true;
    }

    // Seek forward button - go forward 3 seconds
    if (IsButtonClicked(ui->seek_forward_btn_rect) && current_state != STATE_INIT) {
        // Compute target frame relative to the absolute current frame
        const double FPS = 20.0;
        int current_frame = ui->current_frame_number;
        int target_frame = current_frame + (int)(3.0 * FPS);
        
        double new_time = (double)target_frame / FPS;
        logger_log("seek forward button clicked - seeking to frame %d (%.1f seconds)", target_frame, new_time);
        rtsp_client_send_seek_frame(ui->client, target_frame);
        
        // Reset frame counter and elapsed_time after seek
        // Server will send frames starting from target_frame
        ui->frame_count = 0;                 // Reset frame counter (server starts from target)
        ui->frame_count_at_seek = target_frame; // Anchor absolute frame number for upcoming frames
        ui->current_frame_number = target_frame;  // Set absolute frame number immediately
        ui->elapsed_time = 0;          // Reset timer to 0
        
        ui->video_ended = false;  // Reset EOF flag after seek (don't stop during seek)
        ui->consecutive_empty_frames = 0;
        ui->last_frame_time = GetTime();
        // Clear cache to discard old frames and start buffering from new position
        rtp_client_clear_cache(ui->rtp);
        // Set auto-play after seek with 0.2s delay
        ui->seek_time_pending = GetTime();
        ui->auto_play_after_seek = true;
    }

    // Stop button - teardown
    if (IsButtonClicked(ui->stop_btn_rect) && current_state != STATE_INIT) {
        logger_log("stop button clicked");
        ui->close_signal = true;
    }

    // Auto-play after seek (0.2s delay)
    if (ui->auto_play_after_seek && current_state != STATE_INIT) {
        double now = GetTime();
        double elapsed = now - ui->seek_time_pending;
        if (elapsed >= 0.2) {
            logger_log("auto-play triggered after 0.2s seek delay");
            // Send PLAY command
            if (current_state == STATE_READY) {
                rtsp_client_send_play(ui->client);
                ui->timer_running = true;
            }
            ui->auto_play_after_seek = false;
        }
    }

    if (current_state == STATE_PLAYING) {
        // Only update video if not buffering
        if (!rtp_client_is_buffering(ui->rtp)) {
            // Adaptive frame timing to stabilize buffer around target level
            double now = GetTime();
            int buffer_level = ui->last_buffer_level;
            
            // Adjust consume rate based on buffer level vs target
            double frame_interval;
            if (buffer_level > BUFFER_TARGET_HIGH) {
                frame_interval = FRAME_INTERVAL_FAST;  // Consume faster to drain buffer
            } else if (buffer_level < BUFFER_TARGET_LOW) {
                frame_interval = FRAME_INTERVAL_SLOW;  // Consume slower to fill buffer
            } else {
                frame_interval = FRAME_INTERVAL_NORMAL;  // Normal rate
            }
            
            if (now - ui->last_frame_time >= frame_interval) {
                size_t frame_size = rtp_client_get_frame(ui->rtp, ui->frame_data_buffer);
                
                if (frame_size > 0) {
                    // Got a frame - reset EOF counter and increment frame count
                    ui->consecutive_empty_frames = 0;
                    ui->frame_count++;
                    // Update absolute frame number: frame_count tương đối từ seek được cộng vào seek position
                    ui->current_frame_number = ui->frame_count_at_seek + ui->frame_count;
                    client_ui_update_video(ui, ui->frame_data_buffer, frame_size);
                } else {
                    // No frame available
                    ui->consecutive_empty_frames++;
                    
                    // If no frames for a while and buffer is empty, video has ended
                    if (ui->consecutive_empty_frames > 30 && buffer_level == 0) {
                        logger_log("video ended - no frames for %.1f seconds, buffer empty", 
                            ui->consecutive_empty_frames * frame_interval);
                        ui->video_ended = true;
                        ui->timer_running = false;
                    }
                }
                
                ui->last_frame_time = now;
            }
        }
    }
}

static void client_ui_draw(client_ui_t *ui) {
    BeginDrawing();
    ClearBackground(UI_BG_COLOR);

    // Draw video (scaled to fit video_rect when window is resized)
    Rectangle source_rect = { 0, 0, ui->video_texture.texture.width, -ui->video_texture.texture.height };
    DrawTexturePro(ui->video_texture.texture, source_rect, ui->video_rect, (Vector2){ 0, 0 }, 0.0f, WHITE);

    // Get current state for button rendering
    pthread_mutex_lock(&ui->client->state_mutex);
    client_state_t current_state = ui->client->state;
    pthread_mutex_unlock(&ui->client->state_mutex);

    // Draw timer bar
    DrawRectangleRec(ui->timer_rect, UI_TIMER_COLOR);

    // Format and draw timer
    int total_seconds = (int)ui->elapsed_time;
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    char timer_text[16];
    sprintf(timer_text, "%02d:%02d", minutes, seconds);

    int timer_font_size = 20;
    int timer_text_width = MeasureText(timer_text, timer_font_size);
    DrawText(timer_text,
        (ui->screen_width - timer_text_width) / 2,
        ui->timer_rect.y + (TIMER_HEIGHT - timer_font_size) / 2,
        timer_font_size, UI_TEXT_COLOR
    );

    // Draw frame number on the right of timer
    char frame_text[32];
    sprintf(frame_text, "Frame: %d", ui->current_frame_number);
    int frame_font_size = 12;
    int frame_text_width = MeasureText(frame_text, frame_font_size);
    DrawText(frame_text,
        ui->screen_width - frame_text_width - 10,
        ui->timer_rect.y + (TIMER_HEIGHT - frame_font_size) / 2,
        frame_font_size, UI_TEXT_DIM_COLOR
    );

    // Draw status indicator on the left of timer
    const char *status_text = "DISCONNECTED";
    Color status_color = UI_TEXT_DIM_COLOR;

    if (current_state == STATE_READY) {
        status_text = "READY";
        status_color = UI_WARNING_COLOR;
    } else if (current_state == STATE_PLAYING) {
        // Check if buffering
        if (rtp_client_is_buffering(ui->rtp)) {
            status_text = "BUFFERING...";
            status_color = UI_WARNING_COLOR;
        } else {
            status_text = "PLAYING";
            status_color = UI_SUCCESS_COLOR;
        }
    }

    DrawText(status_text, 15, ui->timer_rect.y + (TIMER_HEIGHT - 14) / 2, 14, status_color);

    // Draw stats bar
    DrawRectangleRec(ui->stats_rect, UI_STATS_COLOR);

    if (current_state != STATE_INIT) {
        // Draw buffer level bar
        float bar_width = 80.0f;
        float bar_height = 10.0f;
        float bar_x = 10.0f;
        float bar_y = ui->stats_rect.y + (STATS_HEIGHT - bar_height) / 2;

        // Background
        DrawRectangle(bar_x, bar_y, bar_width, bar_height, UI_DISABLED_COLOR);

        // Fill based on buffer level
        float fill_width = (bar_width * ui->last_buffer_level) / 100.0f;
        Color fill_color = ui->last_buffer_level > 50 ? UI_SUCCESS_COLOR :
                          (ui->last_buffer_level > 20 ? UI_WARNING_COLOR : UI_DANGER_COLOR);
        DrawRectangle(bar_x, bar_y, fill_width, bar_height, fill_color);

        // Buffer label
        char buf_text[32];
        sprintf(buf_text, "BUF: %d%%", ui->last_buffer_level);
        DrawText(buf_text, bar_x + bar_width + 8, ui->stats_rect.y + (STATS_HEIGHT - 12) / 2, 12, UI_TEXT_DIM_COLOR);

        // Packet stats
        char stats_text[64];
        sprintf(stats_text, "Pkts: %u  Lost: %u",
            ui->last_stats.packets_received,
            ui->last_stats.packets_lost      
        );
        int stats_width = MeasureText(stats_text, 12);
        DrawText(stats_text, ui->screen_width - stats_width - 10,
            ui->stats_rect.y + (STATS_HEIGHT - 12) / 2, 12, UI_TEXT_DIM_COLOR);
    }

    // Draw toolbar background
    DrawRectangleRec(ui->toolbar_rect, UI_TOOLBAR_COLOR);

    // Draw buttons
    float icon_size = 30.0f;

    // Connect button
    bool connect_enabled = (current_state == STATE_INIT);
    bool connect_hovered = IsMouseOver(ui->connect_btn_rect) && connect_enabled;
    Color connect_color = connect_enabled ? UI_ACCENT_COLOR : UI_DISABLED_COLOR;
    Color connect_hover = UI_ACCENT_HOVER;

    DrawRoundedButton(ui->connect_btn_rect, connect_color, connect_hover, connect_enabled, connect_hovered);
    DrawConnectIcon(
        ui->connect_btn_rect.x + ui->connect_btn_rect.width / 2,
        ui->connect_btn_rect.y + ui->connect_btn_rect.height / 2,
        icon_size,
        connect_enabled ? UI_TEXT_COLOR : UI_TEXT_DIM_COLOR
    );

    // Play/Pause button
    bool playpause_enabled = (current_state == STATE_READY || current_state == STATE_PLAYING);
    bool playpause_hovered = IsMouseOver(ui->playpause_btn_rect) && playpause_enabled;
    Color playpause_color = playpause_enabled ? UI_ACCENT_COLOR : UI_DISABLED_COLOR;

    DrawRoundedButton(ui->playpause_btn_rect, playpause_color, UI_ACCENT_HOVER, playpause_enabled, playpause_hovered);

    float pp_cx = ui->playpause_btn_rect.x + ui->playpause_btn_rect.width / 2;
    float pp_cy = ui->playpause_btn_rect.y + ui->playpause_btn_rect.height / 2;
    Color pp_icon_color = playpause_enabled ? UI_TEXT_COLOR : UI_TEXT_DIM_COLOR;

    if (current_state == STATE_PLAYING) {
        DrawPauseIcon(pp_cx, pp_cy, icon_size, pp_icon_color);
    } else {
        DrawPlayIcon(pp_cx, pp_cy, icon_size, pp_icon_color);
    }

    // Seek back button
    bool seek_back_enabled = (current_state != STATE_INIT);
    bool seek_back_hovered = IsMouseOver(ui->seek_back_btn_rect) && seek_back_enabled;
    Color seek_back_color = seek_back_enabled ? UI_ACCENT_COLOR : UI_DISABLED_COLOR;

    DrawRoundedButton(ui->seek_back_btn_rect, seek_back_color, UI_ACCENT_HOVER, seek_back_enabled, seek_back_hovered);
    DrawSeekBackIcon(
        ui->seek_back_btn_rect.x + ui->seek_back_btn_rect.width / 2,
        ui->seek_back_btn_rect.y + ui->seek_back_btn_rect.height / 2,
        icon_size,
        seek_back_enabled ? UI_TEXT_COLOR : UI_TEXT_DIM_COLOR
    );

    // Seek forward button
    bool seek_forward_enabled = (current_state != STATE_INIT);
    bool seek_forward_hovered = IsMouseOver(ui->seek_forward_btn_rect) && seek_forward_enabled;
    Color seek_forward_color = seek_forward_enabled ? UI_ACCENT_COLOR : UI_DISABLED_COLOR;

    DrawRoundedButton(ui->seek_forward_btn_rect, seek_forward_color, UI_ACCENT_HOVER, seek_forward_enabled, seek_forward_hovered);
    DrawSeekForwardIcon(
        ui->seek_forward_btn_rect.x + ui->seek_forward_btn_rect.width / 2,
        ui->seek_forward_btn_rect.y + ui->seek_forward_btn_rect.height / 2,
        icon_size,
        seek_forward_enabled ? UI_TEXT_COLOR : UI_TEXT_DIM_COLOR
    );

    // Stop button
    bool stop_enabled = (current_state != STATE_INIT);
    bool stop_hovered = IsMouseOver(ui->stop_btn_rect) && stop_enabled;

    DrawRoundedButton(ui->stop_btn_rect, stop_enabled ? UI_DANGER_COLOR : UI_DISABLED_COLOR,
                      UI_DANGER_HOVER, stop_enabled, stop_hovered);
    DrawStopIcon(
        ui->stop_btn_rect.x + ui->stop_btn_rect.width / 2,
        ui->stop_btn_rect.y + ui->stop_btn_rect.height / 2,
        icon_size,
        stop_enabled ? UI_TEXT_COLOR : UI_TEXT_DIM_COLOR
    );

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

    // Send TEARDOWN if we are connected
    if (ui->client->state != STATE_INIT) {
        rtsp_client_send_teardown(ui->client);
    }

    // Stop network threads
    rtp_client_stop_listener(ui->rtp);
    rtsp_client_disconnect(ui->client);

    // Free raylib resources
    UnloadRenderTexture(ui->video_texture);
    free(ui->frame_data_buffer);

    CloseWindow();
}
