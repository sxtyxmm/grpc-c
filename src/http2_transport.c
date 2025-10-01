/**
 * @file http2_transport.c
 * @brief HTTP/2 transport layer implementation
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

/* HTTP/2 connection preface */
static const uint8_t HTTP2_CLIENT_PREFACE[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
static const size_t HTTP2_CLIENT_PREFACE_LEN = 24;

/* HTTP/2 frame header size */
#define HTTP2_FRAME_HEADER_SIZE 9

/* Default HTTP/2 settings */
#define HTTP2_DEFAULT_WINDOW_SIZE 65535
#define HTTP2_DEFAULT_MAX_FRAME_SIZE 16384

/* ========================================================================
 * HTTP/2 Connection Implementation
 * ======================================================================== */

http2_connection *http2_connection_create(const char *target, bool is_client, void *ssl_ctx) {
    http2_connection *conn = (http2_connection *)calloc(1, sizeof(http2_connection));
    if (!conn) {
        return NULL;
    }
    
    conn->is_client = is_client;
    conn->next_stream_id = is_client ? 1 : 2;
    conn->socket_fd = -1; /* Initialize to invalid */
    pthread_mutex_init(&conn->write_mutex, NULL);
    pthread_mutex_init(&conn->streams_mutex, NULL);
    
    conn->streams_capacity = 16;
    conn->streams = (http2_stream **)calloc(conn->streams_capacity, sizeof(http2_stream *));
    if (!conn->streams) {
        free(conn);
        return NULL;
    }
    
    /* Initialize flow control */
    http2_flow_control_init_connection(conn);
    
    /* For client connections, delay actual connection until first use (lazy connection) */
    /* This allows creating channels without requiring the server to be running */
    (void)target; /* Unused for now - would be used for lazy connection */
    (void)ssl_ctx; /* Unused for now - would be used for TLS */
    
    return conn;
}

void http2_connection_destroy(http2_connection *conn) {
    if (!conn) return;
    
    pthread_mutex_lock(&conn->streams_mutex);
    for (size_t i = 0; i < conn->streams_count; i++) {
        http2_stream_destroy(conn->streams[i]);
    }
    free(conn->streams);
    pthread_mutex_unlock(&conn->streams_mutex);
    
    if (conn->socket_fd >= 0) {
        close(conn->socket_fd);
    }
    
    pthread_mutex_destroy(&conn->write_mutex);
    pthread_mutex_destroy(&conn->streams_mutex);
    free(conn);
}

int http2_connection_send_frame(http2_connection *conn, const http2_frame_header *header, const uint8_t *payload) {
    if (!conn || !header) {
        return -1;
    }
    
    /* Encode frame header */
    uint8_t frame_header[HTTP2_FRAME_HEADER_SIZE];
    frame_header[0] = (header->length >> 16) & 0xFF;
    frame_header[1] = (header->length >> 8) & 0xFF;
    frame_header[2] = header->length & 0xFF;
    frame_header[3] = header->type;
    frame_header[4] = header->flags;
    frame_header[5] = (header->stream_id >> 24) & 0x7F; /* Clear reserved bit */
    frame_header[6] = (header->stream_id >> 16) & 0xFF;
    frame_header[7] = (header->stream_id >> 8) & 0xFF;
    frame_header[8] = header->stream_id & 0xFF;
    
    pthread_mutex_lock(&conn->write_mutex);
    
    /* Send frame header */
    ssize_t sent = send(conn->socket_fd, frame_header, HTTP2_FRAME_HEADER_SIZE, 0);
    if (sent != HTTP2_FRAME_HEADER_SIZE) {
        pthread_mutex_unlock(&conn->write_mutex);
        return -1;
    }
    
    /* Send payload if present */
    if (header->length > 0 && payload) {
        sent = send(conn->socket_fd, payload, header->length, 0);
        if (sent != (ssize_t)header->length) {
            pthread_mutex_unlock(&conn->write_mutex);
            return -1;
        }
    }
    
    pthread_mutex_unlock(&conn->write_mutex);
    return 0;
}

int http2_connection_recv_frame(http2_connection *conn, http2_frame_header *header, uint8_t **payload) {
    if (!conn || !header) {
        return -1;
    }
    
    /* Receive frame header */
    uint8_t frame_header[HTTP2_FRAME_HEADER_SIZE];
    ssize_t received = recv(conn->socket_fd, frame_header, HTTP2_FRAME_HEADER_SIZE, MSG_WAITALL);
    if (received != HTTP2_FRAME_HEADER_SIZE) {
        return -1;
    }
    
    /* Decode frame header */
    header->length = (frame_header[0] << 16) | (frame_header[1] << 8) | frame_header[2];
    header->type = frame_header[3];
    header->flags = frame_header[4];
    header->stream_id = ((frame_header[5] & 0x7F) << 24) | (frame_header[6] << 16) | 
                        (frame_header[7] << 8) | frame_header[8];
    
    /* Receive payload if present */
    if (header->length > 0 && payload) {
        *payload = (uint8_t *)malloc(header->length);
        if (!*payload) {
            return -1;
        }
        
        received = recv(conn->socket_fd, *payload, header->length, MSG_WAITALL);
        if (received != (ssize_t)header->length) {
            free(*payload);
            *payload = NULL;
            return -1;
        }
    } else if (payload) {
        *payload = NULL;
    }
    
    return 0;
}

/* ========================================================================
 * HTTP/2 Stream Implementation
 * ======================================================================== */

http2_stream *http2_stream_create(http2_connection *conn, uint32_t stream_id) {
    if (!conn) {
        return NULL;
    }
    
    http2_stream *stream = (http2_stream *)calloc(1, sizeof(http2_stream));
    if (!stream) {
        return NULL;
    }
    
    stream->id = stream_id;
    stream->conn = conn;
    stream->headers_sent = false;
    stream->end_stream_sent = false;
    stream->end_stream_received = false;
    stream->status = GRPC_STATUS_OK;
    
    /* Initialize flow control */
    http2_flow_control_init_stream(stream);
    
    /* Add stream to connection */
    pthread_mutex_lock(&conn->streams_mutex);
    if (conn->streams_count >= conn->streams_capacity) {
        size_t new_capacity = conn->streams_capacity * 2;
        http2_stream **new_streams = (http2_stream **)realloc(conn->streams, 
                                                               new_capacity * sizeof(http2_stream *));
        if (!new_streams) {
            pthread_mutex_unlock(&conn->streams_mutex);
            free(stream);
            return NULL;
        }
        conn->streams = new_streams;
        conn->streams_capacity = new_capacity;
    }
    conn->streams[conn->streams_count++] = stream;
    pthread_mutex_unlock(&conn->streams_mutex);
    
    return stream;
}

void http2_stream_destroy(http2_stream *stream) {
    if (!stream) return;
    
    if (stream->recv_buffer) {
        grpc_byte_buffer_destroy(stream->recv_buffer);
    }
    
    if (stream->initial_metadata.metadata) {
        free(stream->initial_metadata.metadata);
    }
    
    if (stream->trailing_metadata.metadata) {
        free(stream->trailing_metadata.metadata);
    }
    
    free(stream->status_details);
    free(stream);
}
