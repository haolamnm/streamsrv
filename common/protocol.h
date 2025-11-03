#ifndef PROTOCOL_H
#define PROTOCOL_H

#define RTSP_VERSION "RTSP/1.0"
#define RTP_HEADER_SIZE 12
#define MJPEG_TYPE 26

typedef enum {
    METHOD_SETUP,
    METHOD_PLAY,
    METHOD_PAUSE,
    METHOD_TEARDOWN,
    METHOD_UNKNOWN
} rtsp_method_t;

typedef enum {
    STATE_INIT,
    STATE_READY,
    STATTE_PLAYING
} client_state_t;

typedef enum {
    STATUS_OK_200 = 0,
    STATUS_FILE_NOT_FOUND = 1,
    STATUS_CON_ERR_500 = 2
} rtsp_status_t;

#endif // PROTOCOL_H
