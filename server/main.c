#include "../common/logger.h"
#include "server_worker.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef __linux__
#include <asm-generic/socket.h> // Linux-only
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 10

int main(int argc, char *argv[]) {
    logger_init(LOG_SRC_SERVER);
    srand(time(NULL));

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int server_port = atoi(argv[1]);
    if (server_port <= 0) {
        fprintf(stderr, "Error: invalid port number\n");
        exit(EXIT_FAILURE);
    }
    logger_log("server starting up");

    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        logger_log("error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR option, allow restarting the server immediately
    // without waiting for the OS to the free the port
    int opt = 1;
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger_log("error setting socket options");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        logger_log("error binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket_fd, BACKLOG) < 0) {
        logger_log("error listening on socket");
        exit(EXIT_FAILURE);
    }

    logger_log("server listening on port %d", server_port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Blocking here, waiting for new client to connect
        int client_socket_fd = accept(
            server_socket_fd, (struct sockaddr *)&client_addr, &client_len
        );
        if (client_socket_fd < 0) {
            logger_log("error accepting connection");
            continue;
        }

        // WHEN CLIENT CONNECTED
        session_t *session = (session_t *)malloc(sizeof(session_t));
        if (session == NULL) {
            logger_log("error allocating memory for session");
            close(client_socket_fd);
            continue;
        }

        session->rtsp_socket_fd = client_socket_fd;
        session->client_addr = client_addr;
        session->state = STATE_INIT;
        session->session_id = 0;
        logger_log("new connection from %s:%d",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port)
        );

        // SPAWN SERVER WORKER THREAD
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, server_worker_thread, (void*)session) != 0) {
            logger_log("error creating client thread");
            close(client_socket_fd);
            free(session);
            continue;
        }

        // This thread will now run independently
        pthread_detach(thread_id);
    }

    // This code is never reached in this design
    close(server_socket_fd);
    logger_log("server shutting down");
    return EXIT_SUCCESS;
}
