# gRPC-C API Documentation

This document provides detailed documentation for the gRPC-C API.

## Table of Contents

1. [Initialization and Shutdown](#initialization-and-shutdown)
2. [Completion Queue](#completion-queue)
3. [Channels](#channels)
4. [Calls](#calls)
5. [Server](#server)
6. [Credentials](#credentials)
7. [Utilities](#utilities)
8. [Status Codes](#status-codes)
9. [Error Handling](#error-handling)

## Initialization and Shutdown

### grpc_init()

Initializes the gRPC library. Must be called before any other gRPC functions.

```c
void grpc_init(void);
```

**Thread Safety**: Safe to call from multiple threads, but should typically be called once at program startup.

### grpc_shutdown()

Shuts down the gRPC library and frees all global resources.

```c
void grpc_shutdown(void);
```

**Note**: All gRPC objects must be destroyed before calling this function.

### grpc_version_string()

Returns the version string of the gRPC-C library.

```c
const char *grpc_version_string(void);
```

**Returns**: A static string containing the version (e.g., "1.0.0").

## Completion Queue

Completion queues are used for asynchronous notification of operation completion.

### grpc_completion_queue_create()

Creates a new completion queue.

```c
grpc_completion_queue *grpc_completion_queue_create(grpc_completion_type type);
```

**Parameters**:
- `type`: The type of completion queue
  - `GRPC_CQ_NEXT`: Returns events in the order they complete
  - `GRPC_CQ_PLUCK`: Allows selective retrieval of specific events

**Returns**: A pointer to the new completion queue, or NULL on failure.

### grpc_completion_queue_next()

Retrieves the next event from the completion queue.

```c
grpc_event grpc_completion_queue_next(grpc_completion_queue *cq, grpc_timespec deadline);
```

**Parameters**:
- `cq`: The completion queue
- `deadline`: Maximum time to wait for an event

**Returns**: A grpc_event structure containing the event details.

**Blocks**: Until an event is available or the deadline expires.

### grpc_completion_queue_shutdown()

Initiates shutdown of the completion queue.

```c
void grpc_completion_queue_shutdown(grpc_completion_queue *cq);
```

**Note**: After calling this, no new events can be added to the queue.

### grpc_completion_queue_destroy()

Destroys the completion queue and frees its resources.

```c
void grpc_completion_queue_destroy(grpc_completion_queue *cq);
```

**Precondition**: The completion queue must be shut down first.

## Channels

Channels represent a connection to a gRPC server.

### grpc_insecure_channel_create()

Creates an insecure channel to a server.

```c
grpc_channel *grpc_insecure_channel_create(const char *target, 
                                            const grpc_channel_args *args);
```

**Parameters**:
- `target`: The server address (e.g., "localhost:50051")
- `args`: Optional channel arguments (can be NULL)

**Returns**: A pointer to the new channel, or NULL on failure.

**Example**:
```c
grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
```

### grpc_channel_create()

Creates a channel with credentials.

```c
grpc_channel *grpc_channel_create(const char *target,
                                   grpc_channel_credentials *creds,
                                   const grpc_channel_args *args);
```

**Parameters**:
- `target`: The server address
- `creds`: Channel credentials (NULL for insecure)
- `args`: Optional channel arguments

**Returns**: A pointer to the new channel, or NULL on failure.

### grpc_channel_destroy()

Destroys a channel and frees its resources.

```c
void grpc_channel_destroy(grpc_channel *channel);
```

**Note**: All calls on this channel must be destroyed first.

## Calls

Calls represent individual RPC invocations.

### grpc_channel_create_call()

Creates a new call on a channel.

```c
grpc_call *grpc_channel_create_call(grpc_channel *channel,
                                     grpc_call *parent_call,
                                     uint32_t propagation_mask,
                                     grpc_completion_queue *cq,
                                     const char *method,
                                     const char *host,
                                     grpc_timespec deadline);
```

**Parameters**:
- `channel`: The channel to create the call on
- `parent_call`: Parent call for server-side streaming (can be NULL)
- `propagation_mask`: Options for propagating call properties
- `cq`: The completion queue for this call
- `method`: The RPC method name (e.g., "/service/Method")
- `host`: The host name (can be NULL)
- `deadline`: The call deadline

**Returns**: A pointer to the new call, or NULL on failure.

### grpc_call_start_batch()

Starts a batch of operations on a call.

```c
grpc_call_error grpc_call_start_batch(grpc_call *call,
                                       const void *ops,
                                       size_t nops,
                                       void *tag);
```

**Parameters**:
- `call`: The call to operate on
- `ops`: Array of operations to perform
- `nops`: Number of operations
- `tag`: User-defined tag for completion notification

**Returns**: GRPC_CALL_OK on success, error code otherwise.

### grpc_call_cancel()

Cancels a call.

```c
grpc_call_error grpc_call_cancel(grpc_call *call);
```

**Parameters**:
- `call`: The call to cancel

**Returns**: GRPC_CALL_OK on success, error code otherwise.

### grpc_call_destroy()

Destroys a call and frees its resources.

```c
void grpc_call_destroy(grpc_call *call);
```

## Server

### grpc_server_create()

Creates a new gRPC server.

```c
grpc_server *grpc_server_create(const grpc_channel_args *args);
```

**Parameters**:
- `args`: Optional server arguments (can be NULL)

**Returns**: A pointer to the new server, or NULL on failure.

### grpc_server_add_insecure_http2_port()

Adds an insecure listening port to the server.

```c
int grpc_server_add_insecure_http2_port(grpc_server *server, const char *addr);
```

**Parameters**:
- `server`: The server
- `addr`: The address to bind to (e.g., "0.0.0.0:50051")

**Returns**: The bound port number, or 0 on failure.

**Note**: Must be called before grpc_server_start().

### grpc_server_register_completion_queue()

Registers a completion queue with the server.

```c
void grpc_server_register_completion_queue(grpc_server *server,
                                            grpc_completion_queue *cq);
```

**Parameters**:
- `server`: The server
- `cq`: The completion queue to register

### grpc_server_start()

Starts the server.

```c
void grpc_server_start(grpc_server *server);
```

**Parameters**:
- `server`: The server to start

**Note**: All ports must be added and completion queues registered before calling this.

### grpc_server_shutdown_and_notify()

Initiates server shutdown.

```c
void grpc_server_shutdown_and_notify(grpc_server *server,
                                      grpc_completion_queue *cq,
                                      void *tag);
```

**Parameters**:
- `server`: The server to shutdown
- `cq`: Completion queue for shutdown notification
- `tag`: Tag for the shutdown event

### grpc_server_destroy()

Destroys the server and frees its resources.

```c
void grpc_server_destroy(grpc_server *server);
```

**Precondition**: The server must be shut down first.

## Credentials

### grpc_ssl_credentials_create()

Creates SSL/TLS credentials for a channel.

```c
grpc_channel_credentials *grpc_ssl_credentials_create(
    const char *pem_root_certs,
    void *pem_key_cert_pair);
```

**Parameters**:
- `pem_root_certs`: Root certificates in PEM format
- `pem_key_cert_pair`: Client key/cert pair (can be NULL)

**Returns**: A pointer to the credentials, or NULL on failure.

### grpc_ssl_server_credentials_create()

Creates SSL/TLS credentials for a server.

```c
grpc_server_credentials *grpc_ssl_server_credentials_create(
    const char *pem_root_certs,
    void *pem_key_cert_pairs,
    size_t num_key_cert_pairs);
```

**Parameters**:
- `pem_root_certs`: Root certificates in PEM format (can be NULL)
- `pem_key_cert_pairs`: Array of server key/cert pairs
- `num_key_cert_pairs`: Number of key/cert pairs

**Returns**: A pointer to the credentials, or NULL on failure.

### grpc_channel_credentials_release()

Releases channel credentials.

```c
void grpc_channel_credentials_release(grpc_channel_credentials *creds);
```

### grpc_server_credentials_release()

Releases server credentials.

```c
void grpc_server_credentials_release(grpc_server_credentials *creds);
```

## Utilities

### grpc_now()

Returns the current time.

```c
grpc_timespec grpc_now(void);
```

**Returns**: A grpc_timespec representing the current time.

### grpc_timeout_milliseconds_to_deadline()

Converts a timeout in milliseconds to an absolute deadline.

```c
grpc_timespec grpc_timeout_milliseconds_to_deadline(int64_t timeout_ms);
```

**Parameters**:
- `timeout_ms`: Timeout in milliseconds

**Returns**: A grpc_timespec representing the deadline.

### grpc_byte_buffer_create()

Creates a byte buffer from data.

```c
grpc_byte_buffer *grpc_byte_buffer_create(const uint8_t *data, size_t length);
```

**Parameters**:
- `data`: The data to copy
- `length`: Length of the data

**Returns**: A pointer to the byte buffer, or NULL on failure.

### grpc_byte_buffer_destroy()

Destroys a byte buffer.

```c
void grpc_byte_buffer_destroy(grpc_byte_buffer *buffer);
```

## Status Codes

gRPC uses standardized status codes:

- `GRPC_STATUS_OK` (0): Success
- `GRPC_STATUS_CANCELLED` (1): Operation was cancelled
- `GRPC_STATUS_UNKNOWN` (2): Unknown error
- `GRPC_STATUS_INVALID_ARGUMENT` (3): Invalid argument
- `GRPC_STATUS_DEADLINE_EXCEEDED` (4): Deadline expired
- `GRPC_STATUS_NOT_FOUND` (5): Resource not found
- `GRPC_STATUS_ALREADY_EXISTS` (6): Resource already exists
- `GRPC_STATUS_PERMISSION_DENIED` (7): Permission denied
- `GRPC_STATUS_RESOURCE_EXHAUSTED` (8): Resource exhausted
- `GRPC_STATUS_FAILED_PRECONDITION` (9): Precondition failed
- `GRPC_STATUS_ABORTED` (10): Operation aborted
- `GRPC_STATUS_OUT_OF_RANGE` (11): Value out of range
- `GRPC_STATUS_UNIMPLEMENTED` (12): Not implemented
- `GRPC_STATUS_INTERNAL` (13): Internal error
- `GRPC_STATUS_UNAVAILABLE` (14): Service unavailable
- `GRPC_STATUS_DATA_LOSS` (15): Data loss
- `GRPC_STATUS_UNAUTHENTICATED` (16): Authentication failed

## Error Handling

All API functions that can fail return error codes or NULL pointers. Always check return values:

```c
grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
if (channel == NULL) {
    fprintf(stderr, "Failed to create channel\n");
    return 1;
}

grpc_call_error err = grpc_call_cancel(call);
if (err != GRPC_CALL_OK) {
    fprintf(stderr, "Failed to cancel call: %d\n", err);
}
```

## Best Practices

1. **Always initialize**: Call `grpc_init()` before using any gRPC functions
2. **Resource cleanup**: Always destroy objects in reverse order of creation
3. **Error checking**: Check all return values
4. **Deadlines**: Always set reasonable deadlines for calls
5. **Thread safety**: Completion queues can be shared across threads
6. **Shutdown sequence**: 
   - Shutdown server
   - Shutdown completion queues
   - Destroy all objects
   - Call `grpc_shutdown()`
