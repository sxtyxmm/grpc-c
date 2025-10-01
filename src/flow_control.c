/**
 * @file flow_control.c
 * @brief HTTP/2 flow control implementation
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default HTTP/2 flow control window size */
#define HTTP2_DEFAULT_WINDOW_SIZE 65535

/**
 * Send a WINDOW_UPDATE frame
 * @param conn HTTP/2 connection
 * @param stream_id Stream ID (0 for connection-level)
 * @param increment Window size increment
 * @return 0 on success, -1 on error
 */
int http2_flow_control_send_window_update(http2_connection *conn, uint32_t stream_id, uint32_t increment) {
    if (!conn || increment == 0 || increment > 0x7FFFFFFF) {
        return -1;
    }
    
    /* Encode WINDOW_UPDATE payload (4 bytes) */
    uint8_t payload[4];
    payload[0] = (increment >> 24) & 0x7F;  /* Clear reserved bit */
    payload[1] = (increment >> 16) & 0xFF;
    payload[2] = (increment >> 8) & 0xFF;
    payload[3] = increment & 0xFF;
    
    /* Create frame header */
    http2_frame_header header;
    header.length = 4;
    header.type = HTTP2_FRAME_WINDOW_UPDATE;
    header.flags = 0;
    header.stream_id = stream_id;
    
    /* Send frame */
    return http2_connection_send_frame(conn, &header, payload);
}

/**
 * Process a received WINDOW_UPDATE frame
 * @param conn HTTP/2 connection
 * @param stream_id Stream ID (0 for connection-level)
 * @param increment Window size increment
 * @return 0 on success, -1 on error
 */
int http2_flow_control_receive_window_update(http2_connection *conn, uint32_t stream_id, uint32_t increment) {
    if (!conn || increment == 0 || increment > 0x7FFFFFFF) {
        return -1;
    }
    
    if (stream_id == 0) {
        /* Connection-level window update */
        pthread_mutex_lock(&conn->write_mutex);
        conn->remote_window_size += increment;
        
        /* Check for overflow */
        if (conn->remote_window_size > 0x7FFFFFFF) {
            pthread_mutex_unlock(&conn->write_mutex);
            return -1;
        }
        pthread_mutex_unlock(&conn->write_mutex);
    } else {
        /* Stream-level window update */
        pthread_mutex_lock(&conn->streams_mutex);
        
        http2_stream *stream = NULL;
        for (size_t i = 0; i < conn->streams_count; i++) {
            if (conn->streams[i]->id == stream_id) {
                stream = conn->streams[i];
                break;
            }
        }
        
        if (stream) {
            stream->remote_window_size += increment;
            
            /* Check for overflow */
            if (stream->remote_window_size > 0x7FFFFFFF) {
                pthread_mutex_unlock(&conn->streams_mutex);
                return -1;
            }
        }
        
        pthread_mutex_unlock(&conn->streams_mutex);
    }
    
    return 0;
}

/**
 * Check if there is enough window size to send data
 * @param conn HTTP/2 connection
 * @param stream Stream to check
 * @param data_len Amount of data to send
 * @return 1 if can send, 0 if blocked, -1 on error
 */
int http2_flow_control_can_send(http2_connection *conn, http2_stream *stream, size_t data_len) {
    if (!conn || !stream) {
        return -1;
    }
    
    pthread_mutex_lock(&conn->write_mutex);
    int32_t conn_window = conn->remote_window_size;
    pthread_mutex_unlock(&conn->write_mutex);
    
    int32_t stream_window = stream->remote_window_size;
    
    /* Both connection and stream windows must have enough space */
    if ((int32_t)data_len <= conn_window && (int32_t)data_len <= stream_window) {
        return 1;
    }
    
    return 0;
}

/**
 * Consume window size when sending data
 * @param conn HTTP/2 connection
 * @param stream Stream that sent data
 * @param data_len Amount of data sent
 * @return 0 on success, -1 on error
 */
int http2_flow_control_consume_send_window(http2_connection *conn, http2_stream *stream, size_t data_len) {
    if (!conn || !stream) {
        return -1;
    }
    
    pthread_mutex_lock(&conn->write_mutex);
    conn->remote_window_size -= data_len;
    pthread_mutex_unlock(&conn->write_mutex);
    
    stream->remote_window_size -= data_len;
    
    return 0;
}

/**
 * Consume window size when receiving data
 * @param conn HTTP/2 connection
 * @param stream Stream that received data
 * @param data_len Amount of data received
 * @return 0 on success, -1 on error
 */
int http2_flow_control_consume_recv_window(http2_connection *conn, http2_stream *stream, size_t data_len) {
    if (!conn || !stream) {
        return -1;
    }
    
    /* Validate data_len to prevent underflow */
    if ((int32_t)data_len < 0 || (int32_t)data_len > HTTP2_DEFAULT_WINDOW_SIZE) {
        return -1;
    }
    
    pthread_mutex_lock(&conn->write_mutex);
    
    /* Check for underflow before subtracting */
    if (conn->local_window_size < (int32_t)data_len) {
        pthread_mutex_unlock(&conn->write_mutex);
        return -1;
    }
    
    conn->local_window_size -= data_len;
    
    /* Send WINDOW_UPDATE if window is getting low (less than 50% remaining) */
    if (conn->local_window_size < HTTP2_DEFAULT_WINDOW_SIZE / 2) {
        uint32_t increment = HTTP2_DEFAULT_WINDOW_SIZE - conn->local_window_size;
        if (increment > 0) {
            http2_flow_control_send_window_update(conn, 0, increment);
            conn->local_window_size = HTTP2_DEFAULT_WINDOW_SIZE;
        }
    }
    pthread_mutex_unlock(&conn->write_mutex);
    
    /* Check stream window for underflow */
    if (stream->local_window_size < (int32_t)data_len) {
        return -1;
    }
    
    stream->local_window_size -= data_len;
    
    /* Send stream-level WINDOW_UPDATE if needed */
    if (stream->local_window_size < HTTP2_DEFAULT_WINDOW_SIZE / 2) {
        uint32_t increment = HTTP2_DEFAULT_WINDOW_SIZE - stream->local_window_size;
        if (increment > 0) {
            http2_flow_control_send_window_update(conn, stream->id, increment);
            stream->local_window_size = HTTP2_DEFAULT_WINDOW_SIZE;
        }
    }
    
    return 0;
}

/**
 * Initialize flow control for a connection
 * @param conn HTTP/2 connection
 */
void http2_flow_control_init_connection(http2_connection *conn) {
    if (!conn) return;
    
    conn->local_window_size = HTTP2_DEFAULT_WINDOW_SIZE;
    conn->remote_window_size = HTTP2_DEFAULT_WINDOW_SIZE;
    conn->max_frame_size = 16384;  /* Default max frame size */
    conn->max_concurrent_streams = 100;  /* Default max concurrent streams */
}

/**
 * Initialize flow control for a stream
 * @param stream HTTP/2 stream
 */
void http2_flow_control_init_stream(http2_stream *stream) {
    if (!stream) return;
    
    stream->local_window_size = HTTP2_DEFAULT_WINDOW_SIZE;
    stream->remote_window_size = HTTP2_DEFAULT_WINDOW_SIZE;
}
