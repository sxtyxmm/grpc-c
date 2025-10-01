/**
 * @file grpc_channel.c
 * @brief Channel implementation for gRPC client
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Channel Implementation
 * ======================================================================== */

grpc_channel *grpc_channel_create(const char *target,
                                   grpc_channel_credentials *creds,
                                   const grpc_channel_args *args) {
    if (!target) {
        return NULL;
    }
    
    grpc_channel *channel = (grpc_channel *)calloc(1, sizeof(grpc_channel));
    if (!channel) {
        return NULL;
    }
    
    channel->target = strdup(target);
    if (!channel->target) {
        free(channel);
        return NULL;
    }
    
    channel->creds = creds;
    channel->args = (grpc_channel_args *)args; /* Cast away const for storage */
    pthread_mutex_init(&channel->mutex, NULL);
    
    /* Create HTTP/2 connection */
    channel->connection = http2_connection_create(target, true, NULL);
    if (!channel->connection) {
        free(channel->target);
        free(channel);
        return NULL;
    }
    
    return channel;
}

grpc_channel *grpc_insecure_channel_create(const char *target,
                                            const grpc_channel_args *args) {
    return grpc_channel_create(target, NULL, args);
}

void grpc_channel_destroy(grpc_channel *channel) {
    if (!channel) return;
    
    pthread_mutex_lock(&channel->mutex);
    
    if (channel->connection) {
        http2_connection_destroy(channel->connection);
    }
    
    free(channel->target);
    pthread_mutex_unlock(&channel->mutex);
    
    pthread_mutex_destroy(&channel->mutex);
    free(channel);
}

/* ========================================================================
 * Call Implementation
 * ======================================================================== */

grpc_call *grpc_channel_create_call(grpc_channel *channel,
                                     grpc_call *parent_call,
                                     uint32_t propagation_mask,
                                     grpc_completion_queue *cq,
                                     const char *method,
                                     const char *host,
                                     grpc_timespec deadline) {
    if (!channel || !cq || !method) {
        return NULL;
    }
    
    /* Suppress unused parameter warnings - these are for future implementation */
    (void)parent_call;
    (void)propagation_mask;
    
    grpc_call *call = (grpc_call *)calloc(1, sizeof(grpc_call));
    if (!call) {
        return NULL;
    }
    
    call->channel = channel;
    call->cq = cq;
    call->method = strdup(method);
    call->host = host ? strdup(host) : NULL;
    call->deadline = deadline;
    call->status = GRPC_STATUS_OK;
    call->cancelled = false;
    pthread_mutex_init(&call->mutex, NULL);
    
    /* Create HTTP/2 stream */
    pthread_mutex_lock(&channel->mutex);
    if (channel->connection) {
        uint32_t stream_id = channel->connection->next_stream_id;
        channel->connection->next_stream_id += 2;
        call->stream = http2_stream_create(channel->connection, stream_id);
        if (call->stream) {
            call->stream->call = call;
        }
    }
    pthread_mutex_unlock(&channel->mutex);
    
    if (!call->stream) {
        free(call->method);
        free(call->host);
        pthread_mutex_destroy(&call->mutex);
        free(call);
        return NULL;
    }
    
    return call;
}

grpc_call_error grpc_call_start_batch(grpc_call *call,
                                       const void *ops,
                                       size_t nops,
                                       void *tag) {
    if (!call || !call->cq) {
        return GRPC_CALL_ERROR;
    }
    
    /* Suppress unused parameter warnings - these are for future implementation */
    (void)ops;
    (void)nops;
    
    /* This is a simplified implementation */
    /* In a real implementation, we would process each operation in the batch */
    
    /* Push completion event */
    grpc_event event;
    event.type = 1; /* GRPC_OP_COMPLETE */
    event.success = true;
    event.tag = tag;
    
    completion_queue_push_event(call->cq, event);
    
    return GRPC_CALL_OK;
}

grpc_call_error grpc_call_cancel(grpc_call *call) {
    if (!call) {
        return GRPC_CALL_ERROR;
    }
    
    pthread_mutex_lock(&call->mutex);
    call->cancelled = true;
    call->status = GRPC_STATUS_CANCELLED;
    pthread_mutex_unlock(&call->mutex);
    
    return GRPC_CALL_OK;
}

void grpc_call_destroy(grpc_call *call) {
    if (!call) return;
    
    pthread_mutex_lock(&call->mutex);
    
    /* Destroy stream if it exists */
    if (call->stream) {
        http2_stream *stream = call->stream;
        call->stream = NULL;
        pthread_mutex_unlock(&call->mutex);
        
        /* Destroy stream outside of call mutex to avoid deadlock */
        http2_stream_destroy(stream);
        
        pthread_mutex_lock(&call->mutex);
    }
    
    free(call->method);
    free(call->host);
    free(call->status_details);
    
    if (call->send_buffer) {
        grpc_byte_buffer_destroy(call->send_buffer);
    }
    
    if (call->recv_buffer) {
        grpc_byte_buffer_destroy(call->recv_buffer);
    }
    
    if (call->initial_metadata.metadata) {
        free(call->initial_metadata.metadata);
    }
    
    if (call->trailing_metadata.metadata) {
        free(call->trailing_metadata.metadata);
    }
    
    pthread_mutex_unlock(&call->mutex);
    pthread_mutex_destroy(&call->mutex);
    free(call);
}
