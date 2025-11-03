#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_SRC_SERVER,
    LOG_SRC_CLIENT
} log_src_t;

void logger_init(log_src_t log_src);

void logger_log(const char *format, ...);

#endif // LOGGER_H
