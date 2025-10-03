# Quick Start Guide for v1.2.0 Features

This guide demonstrates how to use the new TLS/SSL, Protocol Buffers, and Streaming RPC features in grpc-c v1.2.0.

## TLS/SSL Support

### Client-Side TLS

```c
#include <grpc/grpc.h>

int main(void) {
    grpc_init();
    
    /* Create SSL credentials */
    grpc_channel_credentials *creds = grpc_ssl_credentials_create(
        NULL,  /* Use system CA certificates */
        NULL   /* No client certificate */
    );
    
    /* Create secure channel */
    grpc_channel *channel = grpc_channel_create(
        "secure.example.com:443",
        creds,
        NULL
    );
    
    /* Use channel for secure communication... */
    
    /* Cleanup */
    grpc_channel_destroy(channel);
    grpc_channel_credentials_release(creds);
    grpc_shutdown();
    
    return 0;
}
```

### Server-Side TLS

```c
#include <grpc/grpc.h>

int main(void) {
    grpc_init();
    
    /* Load server certificate and key */
    const char *server_cert = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----";
    const char *server_key = "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----";
    
    grpc_ssl_pem_key_cert_pair pair;
    pair.cert_chain = server_cert;
    pair.private_key = server_key;
    
    /* Create server credentials */
    grpc_server_credentials *creds = grpc_ssl_server_credentials_create(
        NULL,  /* No client CA (no mTLS) */
        &pair,
        1      /* One key-cert pair */
    );
    
    /* Create server */
    grpc_server *server = grpc_server_create(NULL);
    
    /* Add secure port */
    int port = grpc_server_add_secure_http2_port(
        server,
        "0.0.0.0:50051",
        creds
    );
    
    if (port > 0) {
        printf("Server listening on secure port %d\n", port);
    }
    
    /* Start server... */
    
    /* Cleanup */
    grpc_server_destroy(server);
    grpc_server_credentials_release(creds);
    grpc_shutdown();
    
    return 0;
}
```

## Protocol Buffers Integration

### Define Your Protocol Buffer

Create a `.proto` file:

```protobuf
syntax = "proto3";

message HelloRequest {
  string name = 1;
}

message HelloResponse {
  string message = 1;
}
```

Compile it:

```bash
protoc-c --c_out=. hello.proto
```

### Use Protocol Buffers in Your Code

```c
#include <grpc/grpc.h>
#include <grpc/grpc_protobuf.h>
#include "hello.pb-c.h"

int main(void) {
    grpc_init();
    
    /* Create a protobuf message */
    HelloRequest request = HELLO_REQUEST__INIT;
    request.name = "World";
    
    /* Serialize to byte buffer */
    grpc_byte_buffer *buffer = grpc_protobuf_serialize((ProtobufCMessage *)&request);
    
    if (buffer) {
        printf("Serialized %zu bytes\n", buffer->length);
        
        /* Send buffer over gRPC... */
        
        /* Deserialize response */
        HelloResponse *response;
        if (grpc_protobuf_deserialize(buffer, &hello_response__descriptor, 
                                      (ProtobufCMessage **)&response) == 0) {
            printf("Response: %s\n", response->message);
            grpc_protobuf_free((ProtobufCMessage *)response);
        }
        
        grpc_byte_buffer_destroy(buffer);
    }
    
    grpc_shutdown();
    return 0;
}
```

## Streaming RPC

### Server Streaming

```c
#include <grpc/grpc.h>

void server_streaming_example(void) {
    grpc_init();
    
    grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    
    /* Create server streaming call */
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(30000);
    grpc_call *call = grpc_channel_create_server_streaming_call(
        channel,
        cq,
        "/myservice/ListItems",
        NULL,
        deadline
    );
    
    /* Send initial request... */
    
    /* Receive multiple responses in a loop */
    while (1) {
        /* Read next message from server */
        grpc_event event = grpc_completion_queue_next(cq, deadline);
        
        if (!event.success) {
            break;  /* Stream ended or error */
        }
        
        /* Process message... */
    }
    
    /* Cleanup */
    grpc_call_destroy(call);
    grpc_completion_queue_destroy(cq);
    grpc_channel_destroy(channel);
    grpc_shutdown();
}
```

### Client Streaming

```c
#include <grpc/grpc.h>

void client_streaming_example(void) {
    grpc_init();
    
    grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    
    /* Create client streaming call */
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(30000);
    grpc_call *call = grpc_channel_create_client_streaming_call(
        channel,
        cq,
        "/myservice/UploadData",
        NULL,
        deadline
    );
    
    /* Send multiple requests in a loop */
    for (int i = 0; i < 10; i++) {
        /* Send message to server... */
    }
    
    /* Signal end of stream (half-close) */
    
    /* Receive final response from server */
    grpc_event event = grpc_completion_queue_next(cq, deadline);
    
    /* Cleanup */
    grpc_call_destroy(call);
    grpc_completion_queue_destroy(cq);
    grpc_channel_destroy(channel);
    grpc_shutdown();
}
```

### Bidirectional Streaming

```c
#include <grpc/grpc.h>

void bidirectional_streaming_example(void) {
    grpc_init();
    
    grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    
    /* Create bidirectional streaming call */
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(60000);
    grpc_call *call = grpc_channel_create_bidi_streaming_call(
        channel,
        cq,
        "/myservice/Chat",
        NULL,
        deadline
    );
    
    /* Both client and server can send and receive concurrently */
    
    /* Example: Send and receive in parallel using threads or async I/O */
    
    /* Thread 1: Send messages */
    while (/* have messages to send */) {
        /* Send message... */
    }
    
    /* Thread 2: Receive messages */
    while (/* stream is open */) {
        grpc_event event = grpc_completion_queue_next(cq, deadline);
        /* Process received message... */
    }
    
    /* Cleanup */
    grpc_call_destroy(call);
    grpc_completion_queue_destroy(cq);
    grpc_channel_destroy(channel);
    grpc_shutdown();
}
```

## Complete Example: Secure Protobuf Streaming

Here's a complete example combining all features:

```c
#include <grpc/grpc.h>
#include <grpc/grpc_protobuf.h>
#include "myservice.pb-c.h"

int main(void) {
    grpc_init();
    
    /* 1. Create secure channel with TLS */
    grpc_channel_credentials *creds = grpc_ssl_credentials_create(NULL, NULL);
    grpc_channel *channel = grpc_channel_create("secure.example.com:443", creds, NULL);
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    
    /* 2. Create server streaming call */
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(30000);
    grpc_call *call = grpc_channel_create_server_streaming_call(
        channel, cq, "/myservice/StreamData", NULL, deadline);
    
    /* 3. Send initial request using protobuf */
    DataRequest request = DATA_REQUEST__INIT;
    request.query = "SELECT * FROM items";
    
    grpc_byte_buffer *req_buffer = grpc_protobuf_serialize((ProtobufCMessage *)&request);
    
    /* Send request... */
    
    /* 4. Receive streaming responses */
    while (1) {
        grpc_event event = grpc_completion_queue_next(cq, deadline);
        
        if (!event.success) break;
        
        /* Deserialize protobuf response */
        DataResponse *response;
        grpc_byte_buffer *resp_buffer = /* get from event */;
        
        if (grpc_protobuf_deserialize(resp_buffer, &data_response__descriptor,
                                      (ProtobufCMessage **)&response) == 0) {
            printf("Received: %s\n", response->data);
            grpc_protobuf_free((ProtobufCMessage *)response);
        }
    }
    
    /* Cleanup */
    grpc_byte_buffer_destroy(req_buffer);
    grpc_call_destroy(call);
    grpc_completion_queue_destroy(cq);
    grpc_channel_destroy(channel);
    grpc_channel_credentials_release(creds);
    grpc_shutdown();
    
    return 0;
}
```

## Building with New Dependencies

Update your Makefile or build command:

```bash
# Compiler flags
CFLAGS += -I/usr/include
LDFLAGS += -lgrpc-c -lssl -lcrypto -lprotobuf-c -lz -lpthread

# Build
gcc -o myapp myapp.c myservice.pb-c.c $(CFLAGS) $(LDFLAGS)
```

## Tips and Best Practices

1. **TLS Certificates**: For production, use proper certificates from a CA. For testing, you can generate self-signed certificates:
   ```bash
   openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
   ```

2. **Error Handling**: Always check return values and handle errors appropriately.

3. **Memory Management**: Remember to free resources:
   - `grpc_byte_buffer_destroy()` for byte buffers
   - `grpc_protobuf_free()` for deserialized protobuf messages
   - `grpc_channel_credentials_release()` for credentials
   - `grpc_call_destroy()` for calls

4. **Streaming**: Use HTTP/2 flow control (handled automatically) to prevent overwhelming receivers.

5. **Deadlines**: Always set appropriate deadlines for calls to prevent hanging.

## Next Steps

- Check the [examples](../examples/) directory for working code
- Read [API.md](API.md) for detailed API documentation
- See [IMPLEMENTATION.md](IMPLEMENTATION.md) for architecture details
- Review tests in [test/](../test/) for usage patterns

## Support

For issues or questions:
- Open an issue on GitHub
- Check existing documentation
- Review the test suite for examples
