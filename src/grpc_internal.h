/**
 * @file grpc_internal.h
 * @brief Internal data structures and definitions for gRPC-C
 */

#ifndef GRPC_INTERNAL_H
#define GRPC_INTERNAL_H

#include "grpc/grpc.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* HTTP/2 frame types */
typedef enum {
    HTTP2_FRAME_DATA = 0x00,
    HTTP2_FRAME_HEADERS = 0x01,
    HTTP2_FRAME_PRIORITY = 0x02,
    HTTP2_FRAME_RST_STREAM = 0x03,
    HTTP2_FRAME_SETTINGS = 0x04,
    HTTP2_FRAME_PUSH_PROMISE = 0x05,
    HTTP2_FRAME_PING = 0x06,
    HTTP2_FRAME_GOAWAY = 0x07,
    HTTP2_FRAME_WINDOW_UPDATE = 0x08,
    HTTP2_FRAME_CONTINUATION = 0x09
} http2_frame_type;

/* HTTP/2 settings */
typedef enum {
    HTTP2_SETTINGS_HEADER_TABLE_SIZE = 0x01,
    HTTP2_SETTINGS_ENABLE_PUSH = 0x02,
    HTTP2_SETTINGS_MAX_CONCURRENT_STREAMS = 0x03,
    HTTP2_SETTINGS_INITIAL_WINDOW_SIZE = 0x04,
    HTTP2_SETTINGS_MAX_FRAME_SIZE = 0x05,
    HTTP2_SETTINGS_MAX_HEADER_LIST_SIZE = 0x06
} http2_settings_id;

/* HTTP/2 frame header */
typedef struct {
    uint32_t length;
    uint8_t type;
    uint8_t flags;
    uint32_t stream_id;
} http2_frame_header;

/* HTTP/2 connection */
typedef struct {
    int socket_fd;
    void *ssl_ctx;
    void *ssl;
    bool is_client;
    uint32_t next_stream_id;
    pthread_mutex_t write_mutex;
    pthread_mutex_t streams_mutex;
    struct http2_stream **streams;
    size_t streams_count;
    size_t streams_capacity;
} http2_connection;

/* HTTP/2 stream */
typedef struct http2_stream {
    uint32_t id;
    http2_connection *conn;
    grpc_call *call;
    bool headers_sent;
    bool end_stream_sent;
    bool end_stream_received;
    grpc_metadata_array initial_metadata;
    grpc_metadata_array trailing_metadata;
    grpc_byte_buffer *recv_buffer;
    grpc_status_code status;
    char *status_details;
} http2_stream;

/* Completion queue implementation */
typedef struct completion_queue_event {
    grpc_event event;
    struct completion_queue_event *next;
} completion_queue_event;

struct grpc_completion_queue {
    grpc_completion_type type;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    completion_queue_event *head;
    completion_queue_event *tail;
    bool shutdown;
};

/* Channel implementation */
struct grpc_channel {
    char *target;
    http2_connection *connection;
    grpc_channel_credentials *creds;
    grpc_channel_args *args;
    pthread_mutex_t mutex;
};

/* Call implementation */
struct grpc_call {
    grpc_channel *channel;
    grpc_server *server;
    http2_stream *stream;
    grpc_completion_queue *cq;
    char *method;
    char *host;
    grpc_timespec deadline;
    grpc_metadata_array initial_metadata;
    grpc_metadata_array trailing_metadata;
    grpc_byte_buffer *send_buffer;
    grpc_byte_buffer *recv_buffer;
    grpc_status_code status;
    char *status_details;
    bool cancelled;
    pthread_mutex_t mutex;
};

/* Server implementation */
typedef struct {
    int socket_fd;
    struct sockaddr_in addr;
    grpc_server_credentials *creds;
} server_port;

struct grpc_server {
    grpc_channel_args *args;
    server_port *ports;
    size_t ports_count;
    size_t ports_capacity;
    grpc_completion_queue **cqs;
    size_t cqs_count;
    size_t cqs_capacity;
    bool started;
    bool shutdown_called;
    pthread_t *worker_threads;
    size_t worker_count;
    pthread_mutex_t mutex;
};

/* Credentials implementation */
struct grpc_channel_credentials {
    char *pem_root_certs;
    void *pem_key_cert_pair;
};

struct grpc_server_credentials {
    char *pem_root_certs;
    void *pem_key_cert_pairs;
    size_t num_key_cert_pairs;
};

/* Internal functions */
http2_connection *http2_connection_create(const char *target, bool is_client, void *ssl_ctx);
void http2_connection_destroy(http2_connection *conn);
int http2_connection_send_frame(http2_connection *conn, const http2_frame_header *header, const uint8_t *payload);
int http2_connection_recv_frame(http2_connection *conn, http2_frame_header *header, uint8_t **payload);

http2_stream *http2_stream_create(http2_connection *conn, uint32_t stream_id);
void http2_stream_destroy(http2_stream *stream);

void completion_queue_push_event(grpc_completion_queue *cq, grpc_event event);

#endif /* GRPC_INTERNAL_H */
