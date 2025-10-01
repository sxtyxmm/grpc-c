/**
 * @file echo_server.c
 * @brief Simple echo server example using gRPC-C
 */

#include "grpc/grpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile int keep_running = 1;

void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(int argc, char **argv) {
    const char *server_address = "0.0.0.0:50051";
    
    if (argc > 1) {
        server_address = argv[1];
    }
    
    printf("Echo Server starting on %s\n", server_address);
    
    /* Initialize gRPC */
    grpc_init();
    
    /* Create server */
    grpc_server *server = grpc_server_create(NULL);
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        grpc_shutdown();
        return 1;
    }
    
    /* Add listening port */
    int port = grpc_server_add_insecure_http2_port(server, server_address);
    if (port == 0) {
        fprintf(stderr, "Failed to add port %s\n", server_address);
        grpc_server_destroy(server);
        grpc_shutdown();
        return 1;
    }
    
    printf("Server listening on port %d\n", port);
    
    /* Create completion queue */
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    if (!cq) {
        fprintf(stderr, "Failed to create completion queue\n");
        grpc_server_destroy(server);
        grpc_shutdown();
        return 1;
    }
    
    /* Register completion queue with server */
    grpc_server_register_completion_queue(server, cq);
    
    /* Start server */
    grpc_server_start(server);
    printf("Server started successfully\n");
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Server loop */
    printf("Press Ctrl+C to stop the server\n\n");
    
    while (keep_running) {
        /* In a real implementation, we would handle incoming calls here */
        sleep(1);
    }
    
    printf("\nShutting down server...\n");
    
    /* Shutdown server */
    grpc_server_shutdown_and_notify(server, cq, NULL);
    grpc_completion_queue_shutdown(cq);
    
    /* Cleanup */
    grpc_completion_queue_destroy(cq);
    grpc_server_destroy(server);
    grpc_shutdown();
    
    printf("Server stopped\n");
    return 0;
}
