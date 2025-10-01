/**
 * @file grpc.h
 * @brief Main public API for gRPC-C library
 * 
 * This header provides the primary interface for the gRPC-C implementation,
 * including client and server APIs, channel management, and call lifecycle.
 */

#ifndef GRPC_C_H
#define GRPC_C_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define GRPC_C_VERSION_MAJOR 1
#define GRPC_C_VERSION_MINOR 1
#define GRPC_C_VERSION_PATCH 0

/* Forward declarations */
typedef struct grpc_channel grpc_channel;
typedef struct grpc_server grpc_server;
typedef struct grpc_call grpc_call;
typedef struct grpc_completion_queue grpc_completion_queue;
typedef struct grpc_metadata grpc_metadata;
typedef struct grpc_byte_buffer grpc_byte_buffer;

/* Status codes (aligned with gRPC specification) */
typedef enum {
    GRPC_STATUS_OK = 0,
    GRPC_STATUS_CANCELLED = 1,
    GRPC_STATUS_UNKNOWN = 2,
    GRPC_STATUS_INVALID_ARGUMENT = 3,
    GRPC_STATUS_DEADLINE_EXCEEDED = 4,
    GRPC_STATUS_NOT_FOUND = 5,
    GRPC_STATUS_ALREADY_EXISTS = 6,
    GRPC_STATUS_PERMISSION_DENIED = 7,
    GRPC_STATUS_RESOURCE_EXHAUSTED = 8,
    GRPC_STATUS_FAILED_PRECONDITION = 9,
    GRPC_STATUS_ABORTED = 10,
    GRPC_STATUS_OUT_OF_RANGE = 11,
    GRPC_STATUS_UNIMPLEMENTED = 12,
    GRPC_STATUS_INTERNAL = 13,
    GRPC_STATUS_UNAVAILABLE = 14,
    GRPC_STATUS_DATA_LOSS = 15,
    GRPC_STATUS_UNAUTHENTICATED = 16
} grpc_status_code;

/* Call error codes */
typedef enum {
    GRPC_CALL_OK = 0,
    GRPC_CALL_ERROR = 1,
    GRPC_CALL_ERROR_NOT_ON_SERVER = 2,
    GRPC_CALL_ERROR_NOT_ON_CLIENT = 3,
    GRPC_CALL_ERROR_ALREADY_INVOKED = 4,
    GRPC_CALL_ERROR_NOT_INVOKED = 5,
    GRPC_CALL_ERROR_ALREADY_FINISHED = 6,
    GRPC_CALL_ERROR_TOO_MANY_OPERATIONS = 7,
    GRPC_CALL_ERROR_INVALID_FLAGS = 8
} grpc_call_error;

/* Completion queue types */
typedef enum {
    GRPC_CQ_NEXT = 0,
    GRPC_CQ_PLUCK = 1
} grpc_completion_type;

/* Completion queue event */
typedef struct {
    int type;
    bool success;
    void *tag;
} grpc_event;

/* Metadata entry */
struct grpc_metadata {
    const char *key;
    const char *value;
    size_t value_length;
};

/* Metadata array */
typedef struct {
    size_t count;
    size_t capacity;
    grpc_metadata *metadata;
} grpc_metadata_array;

/* Byte buffer */
struct grpc_byte_buffer {
    uint8_t *data;
    size_t length;
    size_t capacity;
};

/* Time specification */
typedef struct {
    int64_t tv_sec;
    int32_t tv_nsec;
} grpc_timespec;

/* Channel arguments */
typedef struct {
    const char *key;
    union {
        const char *string;
        int integer;
    } value;
    bool is_string;
} grpc_arg;

typedef struct {
    size_t num_args;
    grpc_arg *args;
} grpc_channel_args;

/* SSL/TLS credentials */
typedef struct grpc_channel_credentials grpc_channel_credentials;
typedef struct grpc_server_credentials grpc_server_credentials;
typedef struct grpc_call_credentials grpc_call_credentials;

/* ========================================================================
 * Library Initialization
 * ======================================================================== */

/**
 * @brief Initialize the gRPC library
 * Must be called before any other gRPC functions
 */
void grpc_init(void);

/**
 * @brief Shutdown the gRPC library and free resources
 * Should be called after all gRPC objects are destroyed
 */
void grpc_shutdown(void);

/* ========================================================================
 * Completion Queue API
 * ======================================================================== */

/**
 * @brief Create a completion queue
 * @param type The type of completion queue (NEXT or PLUCK)
 * @return Pointer to the created completion queue, or NULL on error
 */
grpc_completion_queue *grpc_completion_queue_create(grpc_completion_type type);

/**
 * @brief Get the next event from the completion queue
 * @param cq The completion queue
 * @param deadline Timeout for waiting
 * @return The next event, or a timeout event
 */
grpc_event grpc_completion_queue_next(grpc_completion_queue *cq, grpc_timespec deadline);

/**
 * @brief Shutdown the completion queue
 * @param cq The completion queue to shutdown
 */
void grpc_completion_queue_shutdown(grpc_completion_queue *cq);

/**
 * @brief Destroy the completion queue and free resources
 * @param cq The completion queue to destroy
 */
void grpc_completion_queue_destroy(grpc_completion_queue *cq);

/* ========================================================================
 * Channel API (Client)
 * ======================================================================== */

/**
 * @brief Create a secure channel with credentials
 * @param target The server address (e.g., "localhost:50051")
 * @param creds The channel credentials (NULL for insecure)
 * @param args Additional channel arguments
 * @return Pointer to the created channel, or NULL on error
 */
grpc_channel *grpc_channel_create(const char *target, 
                                   grpc_channel_credentials *creds,
                                   const grpc_channel_args *args);

/**
 * @brief Create an insecure channel
 * @param target The server address
 * @param args Additional channel arguments
 * @return Pointer to the created channel, or NULL on error
 */
grpc_channel *grpc_insecure_channel_create(const char *target,
                                            const grpc_channel_args *args);

/**
 * @brief Destroy a channel and free resources
 * @param channel The channel to destroy
 */
void grpc_channel_destroy(grpc_channel *channel);

/* ========================================================================
 * Call API
 * ======================================================================== */

/**
 * @brief Create a call on a channel
 * @param channel The channel to create the call on
 * @param parent_call Parent call for server-side streaming (can be NULL)
 * @param propagation_mask Propagation options
 * @param cq The completion queue for this call
 * @param method The RPC method name
 * @param host The host name (can be NULL)
 * @param deadline The call deadline
 * @return Pointer to the created call, or NULL on error
 */
grpc_call *grpc_channel_create_call(grpc_channel *channel,
                                     grpc_call *parent_call,
                                     uint32_t propagation_mask,
                                     grpc_completion_queue *cq,
                                     const char *method,
                                     const char *host,
                                     grpc_timespec deadline);

/**
 * @brief Start a batch of operations on a call
 * @param call The call to operate on
 * @param ops Array of operations to perform
 * @param nops Number of operations
 * @param tag Tag to associate with this batch
 * @return GRPC_CALL_OK on success, error code otherwise
 */
grpc_call_error grpc_call_start_batch(grpc_call *call,
                                       const void *ops,
                                       size_t nops,
                                       void *tag);

/**
 * @brief Cancel a call
 * @param call The call to cancel
 * @return GRPC_CALL_OK on success, error code otherwise
 */
grpc_call_error grpc_call_cancel(grpc_call *call);

/**
 * @brief Destroy a call and free resources
 * @param call The call to destroy
 */
void grpc_call_destroy(grpc_call *call);

/* ========================================================================
 * Server API
 * ======================================================================== */

/**
 * @brief Create a server
 * @param args Server arguments
 * @return Pointer to the created server, or NULL on error
 */
grpc_server *grpc_server_create(const grpc_channel_args *args);

/**
 * @brief Add a listening port to the server
 * @param server The server
 * @param addr The address to bind to (e.g., "0.0.0.0:50051")
 * @param creds Server credentials (NULL for insecure)
 * @return The bound port number, or 0 on error
 */
int grpc_server_add_insecure_http2_port(grpc_server *server, const char *addr);

/**
 * @brief Add a secure listening port to the server
 * @param server The server
 * @param addr The address to bind to
 * @param creds Server credentials
 * @return The bound port number, or 0 on error
 */
int grpc_server_add_secure_http2_port(grpc_server *server,
                                       const char *addr,
                                       grpc_server_credentials *creds);

/**
 * @brief Register a completion queue with the server
 * @param server The server
 * @param cq The completion queue to register
 */
void grpc_server_register_completion_queue(grpc_server *server,
                                            grpc_completion_queue *cq);

/**
 * @brief Start the server
 * @param server The server to start
 */
void grpc_server_start(grpc_server *server);

/**
 * @brief Request a new call on the server
 * @param server The server
 * @param call Output parameter for the call
 * @param details Output parameter for call details
 * @param cq The completion queue
 * @param tag Tag for this operation
 * @return GRPC_CALL_OK on success, error code otherwise
 */
grpc_call_error grpc_server_request_call(grpc_server *server,
                                          grpc_call **call,
                                          void *details,
                                          grpc_completion_queue *cq,
                                          void *tag);

/**
 * @brief Shutdown the server
 * @param server The server to shutdown
 * @param cq The completion queue
 * @param tag Tag for this operation
 */
void grpc_server_shutdown_and_notify(grpc_server *server,
                                      grpc_completion_queue *cq,
                                      void *tag);

/**
 * @brief Destroy the server and free resources
 * @param server The server to destroy
 */
void grpc_server_destroy(grpc_server *server);

/* ========================================================================
 * Credentials API
 * ======================================================================== */

/**
 * @brief Create SSL/TLS channel credentials
 * @param pem_root_certs Root certificates in PEM format
 * @param pem_key_cert_pair Client key and certificate (can be NULL)
 * @return Pointer to credentials, or NULL on error
 */
grpc_channel_credentials *grpc_ssl_credentials_create(
    const char *pem_root_certs,
    void *pem_key_cert_pair);

/**
 * @brief Create SSL/TLS server credentials
 * @param pem_root_certs Root certificates in PEM format (can be NULL)
 * @param pem_key_cert_pairs Array of server key/cert pairs
 * @param num_key_cert_pairs Number of key/cert pairs
 * @return Pointer to credentials, or NULL on error
 */
grpc_server_credentials *grpc_ssl_server_credentials_create(
    const char *pem_root_certs,
    void *pem_key_cert_pairs,
    size_t num_key_cert_pairs);

/**
 * @brief Release channel credentials
 * @param creds The credentials to release
 */
void grpc_channel_credentials_release(grpc_channel_credentials *creds);

/**
 * @brief Release server credentials
 * @param creds The credentials to release
 */
void grpc_server_credentials_release(grpc_server_credentials *creds);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * @brief Get current time
 * @return Current time as grpc_timespec
 */
grpc_timespec grpc_now(void);

/**
 * @brief Create a deadline from now + timeout
 * @param timeout_ms Timeout in milliseconds
 * @return Deadline as grpc_timespec
 */
grpc_timespec grpc_timeout_milliseconds_to_deadline(int64_t timeout_ms);

/**
 * @brief Create a byte buffer from data
 * @param data The data to copy
 * @param length The length of the data
 * @return Pointer to the byte buffer, or NULL on error
 */
grpc_byte_buffer *grpc_byte_buffer_create(const uint8_t *data, size_t length);

/**
 * @brief Destroy a byte buffer and free resources
 * @param buffer The buffer to destroy
 */
void grpc_byte_buffer_destroy(grpc_byte_buffer *buffer);

/**
 * @brief Get the library version string
 * @return Version string
 */
const char *grpc_version_string(void);

/* ========================================================================
 * Enhanced Features (v1.1+)
 * ======================================================================== */

/**
 * @brief Add metadata to a metadata array
 * @param array The metadata array
 * @param key The metadata key
 * @param value The metadata value
 * @param value_len Length of the value
 * @return 0 on success, -1 on error
 */
int grpc_metadata_array_add(grpc_metadata_array *array, const char *key, 
                             const char *value, size_t value_len);

/**
 * @brief Initialize a metadata array
 * @param array The metadata array to initialize
 * @param initial_capacity Initial capacity (0 for default)
 * @return 0 on success, -1 on error
 */
int grpc_metadata_array_init(grpc_metadata_array *array, size_t initial_capacity);

/**
 * @brief Cleanup a metadata array
 * @param array The metadata array to cleanup
 */
void grpc_metadata_array_destroy(grpc_metadata_array *array);

/**
 * @brief Compress data
 * @param input Input data
 * @param input_len Length of input
 * @param output Output buffer (allocated by function)
 * @param output_len Length of output
 * @param algorithm Compression algorithm ("gzip", "deflate", "identity")
 * @return 0 on success, -1 on error
 */
int grpc_compress(const uint8_t *input, size_t input_len, 
                  uint8_t **output, size_t *output_len, 
                  const char *algorithm);

/**
 * @brief Decompress data
 * @param input Compressed input data
 * @param input_len Length of input
 * @param output Output buffer (allocated by function)
 * @param output_len Length of output
 * @param algorithm Compression algorithm ("gzip", "deflate", "identity")
 * @return 0 on success, -1 on error
 */
int grpc_decompress(const uint8_t *input, size_t input_len,
                    uint8_t **output, size_t *output_len,
                    const char *algorithm);

/**
 * @brief Create a streaming call (server streaming)
 * @param channel The channel
 * @param cq Completion queue
 * @param method RPC method name
 * @param host Host name (can be NULL)
 * @param deadline Call deadline
 * @return Pointer to the call, or NULL on error
 */
grpc_call *grpc_channel_create_server_streaming_call(grpc_channel *channel,
                                                       grpc_completion_queue *cq,
                                                       const char *method,
                                                       const char *host,
                                                       grpc_timespec deadline);

/**
 * @brief Create a streaming call (client streaming)
 * @param channel The channel
 * @param cq Completion queue
 * @param method RPC method name
 * @param host Host name (can be NULL)
 * @param deadline Call deadline
 * @return Pointer to the call, or NULL on error
 */
grpc_call *grpc_channel_create_client_streaming_call(grpc_channel *channel,
                                                       grpc_completion_queue *cq,
                                                       const char *method,
                                                       const char *host,
                                                       grpc_timespec deadline);

/**
 * @brief Create a bidirectional streaming call
 * @param channel The channel
 * @param cq Completion queue
 * @param method RPC method name
 * @param host Host name (can be NULL)
 * @param deadline Call deadline
 * @return Pointer to the call, or NULL on error
 */
grpc_call *grpc_channel_create_bidi_streaming_call(grpc_channel *channel,
                                                     grpc_completion_queue *cq,
                                                     const char *method,
                                                     const char *host,
                                                     grpc_timespec deadline);

/**
 * @brief Check server health
 * @param channel The channel to check
 * @param service Service name (empty string for overall health)
 * @return 0 if healthy, -1 if unhealthy or error
 */
int grpc_health_check(grpc_channel *channel, const char *service);

#ifdef __cplusplus
}
#endif

#endif /* GRPC_C_H */
