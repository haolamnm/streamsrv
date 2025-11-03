#include "logger.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

// Set once by logger init
static log_src_t g_log_src;

// A mutex is required to prevent multiple threads from writing
// to stdout at the same time
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void logger_init(log_src_t log_src) {
    g_log_src = log_src;
}

void logger_log(const char *format, ...) {
    // Lock the mutex to ensure this log message is printed atomically
    pthread_mutex_lock(&g_log_mutex);

    if (g_log_src == LOG_SRC_SERVER) {
        printf("[S] ");
    } else {
        printf("[C] ");
    }
    printf("[T %lu] ", (unsigned long)pthread_self());

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // vprintf doesn't automatically add a newline
    printf("\n");
    fflush(stdout);

    // Release the lock
    pthread_mutex_unlock(&g_log_mutex);
}
