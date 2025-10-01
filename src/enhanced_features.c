/**
 * @file enhanced_features.c
 * @brief Implementation of enhanced gRPC features (metadata, streaming, health check)
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Metadata API Enhancements
 * ======================================================================== */

/**
 * Initialize a metadata array
 */
int grpc_metadata_array_init(grpc_metadata_array *array, size_t initial_capacity) {
    if (!array) {
        return -1;
    }
    
    array->count = 0;
    array->capacity = initial_capacity > 0 ? initial_capacity : 16;
    array->metadata = (grpc_metadata *)calloc(array->capacity, sizeof(grpc_metadata));
    
    if (!array->metadata) {
        return -1;
    }
    
    return 0;
}

/**
 * Add metadata to array
 */
int grpc_metadata_array_add(grpc_metadata_array *array, const char *key, 
                             const char *value, size_t value_len) {
    if (!array || !key || !value) {
        return -1;
    }
    
    /* Grow array if needed */
    if (array->count >= array->capacity) {
        size_t new_capacity = array->capacity * 2;
        grpc_metadata *new_metadata = (grpc_metadata *)realloc(array->metadata,
                                                                 new_capacity * sizeof(grpc_metadata));
        if (!new_metadata) {
            return -1;
        }
        array->metadata = new_metadata;
        array->capacity = new_capacity;
    }
    
    /* Allocate and copy key */
    char *key_copy = strdup(key);
    if (!key_copy) {
        return -1;
    }
    
    /* Allocate and copy value */
    char *value_copy = (char *)malloc(value_len + 1);
    if (!value_copy) {
        free(key_copy);
        return -1;
    }
    memcpy(value_copy, value, value_len);
    value_copy[value_len] = '\0';
    
    /* Add to array */
    array->metadata[array->count].key = key_copy;
    array->metadata[array->count].value = value_copy;
    array->metadata[array->count].value_length = value_len;
    array->count++;
    
    return 0;
}

/**
 * Cleanup metadata array
 */
void grpc_metadata_array_destroy(grpc_metadata_array *array) {
    if (!array || !array->metadata) {
        return;
    }
    
    for (size_t i = 0; i < array->count; i++) {
        free((void *)array->metadata[i].key);
        free((void *)array->metadata[i].value);
    }
    
    free(array->metadata);
    array->metadata = NULL;
    array->count = 0;
    array->capacity = 0;
}

/* ========================================================================
 * Compression API Wrappers
 * ======================================================================== */

/**
 * Compress data using specified algorithm
 */
int grpc_compress(const uint8_t *input, size_t input_len, 
                  uint8_t **output, size_t *output_len, 
                  const char *algorithm) {
    return grpc_compress_data(input, input_len, output, output_len, algorithm);
}

/**
 * Decompress data using specified algorithm
 */
int grpc_decompress(const uint8_t *input, size_t input_len,
                    uint8_t **output, size_t *output_len,
                    const char *algorithm) {
    return grpc_decompress_data(input, input_len, output, output_len, algorithm);
}

/* ========================================================================
 * Streaming Call Creation
 * ======================================================================== */

/**
 * Create server streaming call
 */
grpc_call *grpc_channel_create_server_streaming_call(grpc_channel *channel,
                                                       grpc_completion_queue *cq,
                                                       const char *method,
                                                       const char *host,
                                                       grpc_timespec deadline) {
    /* Server streaming uses the same call creation as unary */
    /* The streaming behavior is determined by the operations performed on the call */
    return grpc_channel_create_call(channel, NULL, 0, cq, method, host, deadline);
}

/**
 * Create client streaming call
 */
grpc_call *grpc_channel_create_client_streaming_call(grpc_channel *channel,
                                                       grpc_completion_queue *cq,
                                                       const char *method,
                                                       const char *host,
                                                       grpc_timespec deadline) {
    /* Client streaming uses the same call creation as unary */
    /* The streaming behavior is determined by the operations performed on the call */
    return grpc_channel_create_call(channel, NULL, 0, cq, method, host, deadline);
}

/**
 * Create bidirectional streaming call
 */
grpc_call *grpc_channel_create_bidi_streaming_call(grpc_channel *channel,
                                                     grpc_completion_queue *cq,
                                                     const char *method,
                                                     const char *host,
                                                     grpc_timespec deadline) {
    /* Bidirectional streaming uses the same call creation as unary */
    /* The streaming behavior is determined by the operations performed on the call */
    return grpc_channel_create_call(channel, NULL, 0, cq, method, host, deadline);
}

/* ========================================================================
 * Health Check Protocol
 * ======================================================================== */

/**
 * Check server health
 * This is a simplified implementation. A full implementation would use
 * the gRPC Health Checking Protocol (grpc.health.v1.Health service)
 */
int grpc_health_check(grpc_channel *channel, const char *service) {
    if (!channel) {
        return -1;
    }
    
    /* Create a completion queue for this health check */
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    if (!cq) {
        return -1;
    }
    
    /* Create a call to the health service */
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(5000);
    grpc_call *call = grpc_channel_create_call(
        channel, NULL, 0, cq,
        "/grpc.health.v1.Health/Check",
        NULL, deadline);
    
    if (!call) {
        grpc_completion_queue_destroy(cq);
        return -1;
    }
    
    /* In a full implementation, we would:
     * 1. Send initial metadata
     * 2. Send a HealthCheckRequest message with the service name
     * 3. Receive the HealthCheckResponse
     * 4. Check the serving status
     * 
     * For now, we just check if we can create the call
     */
    
    /* Cleanup */
    grpc_call_destroy(call);
    grpc_completion_queue_shutdown(cq);
    grpc_completion_queue_destroy(cq);
    
    /* Return success if we could create the call */
    return 0;
}
