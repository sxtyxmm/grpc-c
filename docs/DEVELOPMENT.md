# Development Guide

This guide helps developers extend and contribute to the grpc-c project.

## Project Organization

```
grpc-c/
+-- include/grpc/       # Public API headers
+-- src/                # Implementation files
+-- test/               # Test suite
+-- examples/           # Example programs
+-- docs/               # Documentation
+-- build/              # Build artifacts (generated)
```

## Building the Project

```bash
# Full build with tests
make all check

# Build only the library
make static    # Static library
make shared    # Shared library

# Build examples
make examples

# Clean build
make clean
```

## Adding New Features

### 1. Extending the API

To add a new API function:

1. Add the declaration to `include/grpc/grpc.h`
2. Implement in the appropriate source file:
   - `src/grpc_core.c` - Core functionality
   - `src/grpc_channel.c` - Client-side operations
   - `src/grpc_server.c` - Server-side operations
   - `src/grpc_credentials.c` - Authentication
   - `src/http2_transport.c` - Transport layer
3. Add tests to `test/basic_test.c` or create a new test file
4. Update documentation in `docs/API.md`

### 2. HTTP/2 Implementation

The HTTP/2 transport layer is in `src/http2_transport.c`. 

**Completed in v1.1:**
- [X] HPACK compression (`src/hpack.c`)
- [X] Flow control (`src/flow_control.c`)

**To complete the implementation:**

1. **Frame Processing**: Add handlers for all HTTP/2 frame types
   - DATA frames for message transfer
   - HEADERS frames for metadata
   - SETTINGS frames for connection parameters
   - WINDOW_UPDATE for flow control (implemented)
   - PING for keepalive
   - GOAWAY for graceful shutdown

2. **Stream Multiplexing**: Handle concurrent streams
   - Stream prioritization
   - Stream dependencies
   - Concurrent stream limits

### 3. Protobuf Integration

To integrate Protocol Buffers:

1. Add protobuf-c as a dependency
2. Create serialization helpers in `src/grpc_protobuf.c`
3. Add API functions for message serialization/deserialization
4. Update byte buffer to work with protobuf messages

Example API:
```c
grpc_byte_buffer *grpc_protobuf_serialize(const ProtobufCMessage *msg);
int grpc_protobuf_deserialize(grpc_byte_buffer *buf, ProtobufCMessage **msg);
```

### 4. TLS/SSL Implementation

To complete TLS support:

1. Add OpenSSL as a dependency
2. Initialize SSL context in `http2_connection_create`
3. Implement SSL handshake
4. Add certificate validation
5. Implement secure read/write operations

Example:
```c
SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_method());
SSL_CTX_load_verify_locations(ssl_ctx, creds->pem_root_certs, NULL);
conn->ssl = SSL_new(ssl_ctx);
SSL_set_fd(conn->ssl, conn->socket_fd);
SSL_connect(conn->ssl);
```

## Code Style

- Use C99 standard
- 4-space indentation
- Opening braces on same line
- Descriptive variable names
- Document all public APIs with Doxygen comments
- Check return values
- Free all allocated memory
- Use const where appropriate

## Testing

### Adding Tests

Add test functions to `test/basic_test.c`:

```c
void test_my_feature(void) {
    TEST(test_my_feature);
    
    // Test setup
    grpc_init();
    
    // Test logic
    if (condition) {
        TEST_PASS();
    } else {
        TEST_FAIL("Reason for failure");
    }
    
    // Cleanup
    grpc_shutdown();
}
```

Add the test to main():
```c
int main(void) {
    // ... existing tests
    test_my_feature();
    // ...
}
```

### Running Tests

```bash
make check
```

## Memory Management

Always follow these rules:

1. **Ownership**: Document who owns each pointer
2. **Cleanup**: Free resources in reverse order of allocation
3. **NULL checks**: Always check allocation results
4. **Leaks**: Test with valgrind: `valgrind --leak-check=full ./bin/basic_test`

## Performance Considerations

1. **Zero-copy**: Minimize data copying where possible
2. **Lock contention**: Use fine-grained locking
3. **Memory pools**: Consider pooling for frequently allocated objects
4. **Batching**: Batch operations to reduce system calls

## Debugging

### Enable Debug Output

Add to `grpc_core.c`:
```c
#define GRPC_DEBUG 1

#ifdef GRPC_DEBUG
#define DEBUG_LOG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif
```

### GDB Debugging

```bash
gdb ./bin/basic_test
(gdb) break grpc_channel_create
(gdb) run
(gdb) backtrace
```

## Benchmarking

To add benchmarks:

1. Create `test/benchmark.c`
2. Measure throughput, latency, memory usage
3. Compare with other gRPC implementations

Example:
```c
void benchmark_unary_calls(void) {
    clock_t start = clock();
    for (int i = 0; i < 10000; i++) {
        // Make unary call
    }
    clock_t end = clock();
    double time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Time for 10000 calls: %.2f seconds\n", time);
}
```

## Platform Support

### Linux
- Uses epoll for I/O multiplexing (future)
- pthread for threading
- Standard POSIX APIs

### macOS
- Uses kqueue for I/O multiplexing (future)
- pthread for threading
- BSD socket APIs

### Windows Support (Future)
- Would require IOCP for async I/O
- Windows threads instead of pthread
- Winsock2 for networking

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Update documentation
6. Submit a pull request

## Resources

- [gRPC Protocol](https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md)
- [HTTP/2 Specification](https://httpwg.org/specs/rfc7540.html)
- [HPACK Specification](https://httpwg.org/specs/rfc7541.html)
- [Protocol Buffers](https://developers.google.com/protocol-buffers)
