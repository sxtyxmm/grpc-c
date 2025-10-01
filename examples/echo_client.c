/**
 * @file echo_client.c
 * @brief Simple echo client example using gRPC-C
 */

#include "grpc/grpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *server_address = "localhost:50051";
    const char *message = "Hello, gRPC!";
    
    if (argc > 1) {
        server_address = argv[1];
    }
    if (argc > 2) {
        message = argv[2];
    }
    
    printf("Echo Client connecting to %s\n", server_address);
    printf("Message: %s\n\n", message);
    
    /* Initialize gRPC */
    grpc_init();
    
    /* Create channel */
    grpc_channel *channel = grpc_insecure_channel_create(server_address, NULL);
    if (!channel) {
        fprintf(stderr, "Failed to create channel\n");
        grpc_shutdown();
        return 1;
    }
    
    printf("Channel created successfully\n");
    
    /* Create completion queue */
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    if (!cq) {
        fprintf(stderr, "Failed to create completion queue\n");
        grpc_channel_destroy(channel);
        grpc_shutdown();
        return 1;
    }
    
    /* Create call */
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(5000);
    grpc_call *call = grpc_channel_create_call(channel, NULL, 0, cq,
                                                 "/echo.Echo/SayHello",
                                                 NULL, deadline);
    if (!call) {
        fprintf(stderr, "Failed to create call\n");
        grpc_completion_queue_destroy(cq);
        grpc_channel_destroy(channel);
        grpc_shutdown();
        return 1;
    }
    
    printf("Call created successfully\n");
    
    /* In a real implementation, we would:
     * 1. Send initial metadata
     * 2. Send message
     * 3. Close send
     * 4. Receive initial metadata
     * 5. Receive message
     * 6. Receive status
     */
    
    printf("Call completed (simplified implementation)\n");
    
    /* Cleanup */
    grpc_call_destroy(call);
    grpc_completion_queue_shutdown(cq);
    grpc_completion_queue_destroy(cq);
    grpc_channel_destroy(channel);
    grpc_shutdown();
    
    printf("Client finished\n");
    return 0;
}
