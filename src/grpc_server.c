/**
 * @file grpc_server.c
 * @brief Server implementation for gRPC
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

/* Server configuration constants */
#define GRPC_DEFAULT_WORKER_THREADS 4
#define GRPC_DEFAULT_LISTEN_BACKLOG 128
#define GRPC_SELECT_TIMEOUT_USEC 100000  /* 100ms */

/* ========================================================================
 * Server Implementation
 * ======================================================================== */

grpc_server *grpc_server_create(const grpc_channel_args *args) {
    grpc_server *server = (grpc_server *)calloc(1, sizeof(grpc_server));
    if (!server) {
        return NULL;
    }
    
    server->args = (grpc_channel_args *)args; /* Cast away const for storage */
    server->ports_capacity = 4;
    server->ports = (server_port *)calloc(server->ports_capacity, sizeof(server_port));
    if (!server->ports) {
        free(server);
        return NULL;
    }
    
    server->cqs_capacity = 4;
    server->cqs = (grpc_completion_queue **)calloc(server->cqs_capacity, 
                                                    sizeof(grpc_completion_queue *));
    if (!server->cqs) {
        free(server->ports);
        free(server);
        return NULL;
    }
    
    server->started = false;
    server->shutdown_called = false;
    pthread_mutex_init(&server->mutex, NULL);
    
    return server;
}

int grpc_server_add_insecure_http2_port(grpc_server *server, const char *addr) {
    if (!server || !addr) {
        return 0;
    }
    
    pthread_mutex_lock(&server->mutex);
    
    if (server->started) {
        pthread_mutex_unlock(&server->mutex);
        return 0;
    }
    
    /* Parse address: host:port */
    char *host = strdup(addr);
    char *colon = strchr(host, ':');
    int port = 50051;
    
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }
    
    /* Create socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        free(host);
        pthread_mutex_unlock(&server->mutex);
        return 0;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* Bind socket */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    
    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "[::]") == 0) {
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host, &serv_addr.sin_addr);
    }
    serv_addr.sin_port = htons(port);
    
    if (bind(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(socket_fd);
        free(host);
        pthread_mutex_unlock(&server->mutex);
        return 0;
    }
    
    /* Listen */
    if (listen(socket_fd, GRPC_DEFAULT_LISTEN_BACKLOG) < 0) {
        close(socket_fd);
        free(host);
        pthread_mutex_unlock(&server->mutex);
        return 0;
    }
    
    /* Add to server ports */
    if (server->ports_count >= server->ports_capacity) {
        size_t new_capacity = server->ports_capacity * 2;
        server_port *new_ports = (server_port *)realloc(server->ports,
                                                         new_capacity * sizeof(server_port));
        if (!new_ports) {
            close(socket_fd);
            free(host);
            pthread_mutex_unlock(&server->mutex);
            return 0;
        }
        server->ports = new_ports;
        server->ports_capacity = new_capacity;
    }
    
    server->ports[server->ports_count].socket_fd = socket_fd;
    server->ports[server->ports_count].addr = serv_addr;
    server->ports[server->ports_count].creds = NULL;
    server->ports_count++;
    
    free(host);
    pthread_mutex_unlock(&server->mutex);
    
    return port;
}

int grpc_server_add_secure_http2_port(grpc_server *server,
                                       const char *addr,
                                       grpc_server_credentials *creds) {
    /* Suppress unused parameter warning - for future implementation */
    (void)creds;
    
    /* For now, just call insecure version */
    /* In production, would set up TLS context */
    return grpc_server_add_insecure_http2_port(server, addr);
}

void grpc_server_register_completion_queue(grpc_server *server,
                                            grpc_completion_queue *cq) {
    if (!server || !cq) {
        return;
    }
    
    pthread_mutex_lock(&server->mutex);
    
    if (server->cqs_count >= server->cqs_capacity) {
        size_t new_capacity = server->cqs_capacity * 2;
        grpc_completion_queue **new_cqs = (grpc_completion_queue **)realloc(
            server->cqs, new_capacity * sizeof(grpc_completion_queue *));
        if (!new_cqs) {
            pthread_mutex_unlock(&server->mutex);
            return;
        }
        server->cqs = new_cqs;
        server->cqs_capacity = new_capacity;
    }
    
    server->cqs[server->cqs_count++] = cq;
    
    pthread_mutex_unlock(&server->mutex);
}

void *server_worker_thread(void *arg) {
    grpc_server *server = (grpc_server *)arg;
    
    /* This is a simplified server loop */
    /* In production, would use epoll/kqueue for multiple connections */
    
    while (!server->shutdown_called) {
        /* Accept connections on all ports */
        for (size_t i = 0; i < server->ports_count; i++) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server->ports[i].socket_fd, &read_fds);
            
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = GRPC_SELECT_TIMEOUT_USEC;
            
            int ret = select(server->ports[i].socket_fd + 1, &read_fds, NULL, NULL, &tv);
            if (ret > 0 && FD_ISSET(server->ports[i].socket_fd, &read_fds)) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server->ports[i].socket_fd,
                                      (struct sockaddr *)&client_addr,
                                      &client_len);
                if (client_fd >= 0) {
                    /* Handle client connection */
                    /* In production, would create HTTP/2 connection and process requests */
                    close(client_fd);
                }
            }
        }
    }
    
    return NULL;
}

void grpc_server_start(grpc_server *server) {
    if (!server) {
        return;
    }
    
    pthread_mutex_lock(&server->mutex);
    
    if (server->started) {
        pthread_mutex_unlock(&server->mutex);
        return;
    }
    
    server->started = true;
    
    /* Start worker threads */
    server->worker_count = GRPC_DEFAULT_WORKER_THREADS;
    server->worker_threads = (pthread_t *)calloc(server->worker_count, sizeof(pthread_t));
    
    for (size_t i = 0; i < server->worker_count; i++) {
        pthread_create(&server->worker_threads[i], NULL, server_worker_thread, server);
    }
    
    pthread_mutex_unlock(&server->mutex);
}

grpc_call_error grpc_server_request_call(grpc_server *server,
                                          grpc_call **call,
                                          void *details,
                                          grpc_completion_queue *cq,
                                          void *tag) {
    if (!server || !call || !cq) {
        return GRPC_CALL_ERROR;
    }
    
    /* Suppress unused parameter warnings - these are for future implementation */
    (void)details;
    (void)tag;
    
    /* This is a simplified implementation */
    /* In production, would wait for incoming calls and create call objects */
    
    return GRPC_CALL_OK;
}

void grpc_server_shutdown_and_notify(grpc_server *server,
                                      grpc_completion_queue *cq,
                                      void *tag) {
    if (!server) {
        return;
    }
    
    pthread_mutex_lock(&server->mutex);
    server->shutdown_called = true;
    pthread_mutex_unlock(&server->mutex);
    
    /* Wait for worker threads */
    if (server->worker_threads) {
        for (size_t i = 0; i < server->worker_count; i++) {
            pthread_join(server->worker_threads[i], NULL);
        }
        free(server->worker_threads);
        server->worker_threads = NULL;
    }
    
    /* Notify completion queue */
    if (cq && tag) {
        grpc_event event;
        event.type = 1;
        event.success = true;
        event.tag = tag;
        completion_queue_push_event(cq, event);
    }
}

void grpc_server_destroy(grpc_server *server) {
    if (!server) return;
    
    pthread_mutex_lock(&server->mutex);
    
    /* Close all listening sockets */
    for (size_t i = 0; i < server->ports_count; i++) {
        if (server->ports[i].socket_fd >= 0) {
            close(server->ports[i].socket_fd);
        }
    }
    free(server->ports);
    free(server->cqs);
    
    pthread_mutex_unlock(&server->mutex);
    pthread_mutex_destroy(&server->mutex);
    free(server);
}
