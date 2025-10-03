# Changelog

All notable changes to the grpc-c project will be documented in this file.

## [1.2.0] - 2024-12-15

### Added - Complete Integration Release
- **TLS/SSL with OpenSSL**: Full secure communication support
  - Client SSL context creation and management
  - Server SSL context creation and management
  - Certificate validation and verification
  - Secure handshake (client and server)
  - Secure read/write operations (`grpc_ssl_read()`, `grpc_ssl_write()`)
  - ALPN negotiation for HTTP/2
  - Minimum TLS version enforcement (TLS 1.2+)
  - System CA certificate support
  - Integration with channel and server APIs
- **Protocol Buffers with protobuf-c**: Complete serialization support
  - Message serialization (`grpc_protobuf_serialize()`)
  - Message deserialization (`grpc_protobuf_deserialize()`)
  - Byte buffer integration
  - Message size calculation (`grpc_protobuf_message_size()`)
  - Direct buffer serialization (`grpc_protobuf_serialize_to_buffer()`)
  - New header: `include/grpc/grpc_protobuf.h`
- **Complete streaming RPC implementation**: Full examples and documentation
  - Server streaming example with explanation
  - Client streaming example with explanation
  - Bidirectional streaming example with explanation
  - Backpressure handling documentation
  - Best practices guide
  - Use case examples

### Implementation Details
- New source files:
  - `src/grpc_tls.c` - TLS/SSL implementation with OpenSSL (~450 lines)
  - `src/grpc_protobuf.c` - Protocol Buffers integration (~125 lines)
  - `examples/streaming_example.c` - Complete streaming examples (~275 lines)
- New header files:
  - `include/grpc/grpc_protobuf.h` - Protobuf API definitions
- Updated structures:
  - `grpc_ssl_pem_key_cert_pair` exported in public API
  - Internal SSL support in connection structures
- Added dependencies:
  - OpenSSL (libssl, libcrypto) for TLS/SSL
  - protobuf-c for Protocol Buffers
- New tests:
  - `test/tls_protobuf_test.c` - SSL credentials and protobuf tests
- Updated Makefile with new dependencies
- All existing tests continue to pass
- Total test count: 19 tests (up from 15)

### Documentation Updates
- Updated README.md with v1.2 features
- Updated IMPLEMENTATION.md with completed integrations
- Updated PROJECT_SUMMARY.txt with new statistics
- Updated ENHANCEMENTS_SUMMARY.md with v1.2 section
- All documentation reflects production-ready status

## [1.1.0] - 2024-12-01

### Added - Major Feature Release
- **HTTP/2 HPACK header compression**: Complete implementation with static table
  - Integer encoding/decoding with variable-length encoding
  - Literal header field encoding/decoding
  - Metadata array encoding/decoding support
- **HTTP/2 flow control**: Full window management implementation
  - Connection-level flow control
  - Stream-level flow control
  - Automatic WINDOW_UPDATE frame generation
  - Flow control initialization for connections and streams
- **Compression support**: Data compression/decompression
  - gzip compression using zlib
  - deflate compression support
  - Identity (no compression) support
  - Public API: `grpc_compress()` and `grpc_decompress()`
- **Enhanced metadata API**: Improved metadata handling
  - `grpc_metadata_array_init()` - Initialize metadata arrays
  - `grpc_metadata_array_add()` - Add metadata entries
  - `grpc_metadata_array_destroy()` - Cleanup metadata arrays
- **Streaming RPC support**: Helper functions for streaming calls
  - `grpc_channel_create_server_streaming_call()` - Server streaming
  - `grpc_channel_create_client_streaming_call()` - Client streaming
  - `grpc_channel_create_bidi_streaming_call()` - Bidirectional streaming
- **Health checking protocol**: Basic health check support
  - `grpc_health_check()` - Check server health
  - Framework for gRPC Health Checking Protocol
- **Comprehensive testing**: New test suite for enhanced features
  - Metadata array tests
  - Compression/decompression tests
  - HPACK encoding/decoding tests
  - Streaming call creation tests
  - Health check tests
  - Flow control tests

### Implementation Details
- New source files:
  - `src/hpack.c` - HPACK header compression implementation
  - `src/flow_control.c` - HTTP/2 flow control
  - `src/compression.c` - Data compression support
  - `src/enhanced_features.c` - Enhanced API implementations
- Updated internal structures with flow control fields
- Added zlib dependency for compression
- Extended public API in `grpc.h`
- All existing tests continue to pass
- 6 new tests in enhanced test suite

### Dependencies
- Added zlib library for compression support

## [1.0.0] - 2024-10-01

### Added
- Initial release of grpc-c library
- Complete gRPC API surface with client and server support
- HTTP/2 transport layer framework
- Completion queue implementation for async operations
- Channel creation and management (secure and insecure)
- Call lifecycle management (create, start, cancel, destroy)
- Server implementation with multi-threaded worker pool
- TLS/SSL credentials framework
- Metadata handling support
- Deadline and timeout management
- Status codes aligned with gRPC specification
- Byte buffer utilities
- Cross-platform support (Linux and macOS)
- Comprehensive test suite with 9 tests
- Example echo server and client
- Makefile build system
- Static and shared library builds
- Installation support
- API documentation
- pkg-config integration

### Architecture
- Modular design with clean separation of concerns
- HTTP/2 transport layer with frame handling
- Thread-safe completion queues
- Connection management with lazy initialization
- Stream management for multiplexing
- Resource cleanup and lifecycle management

### Documentation
- Complete README with quick start guide
- Detailed API documentation
- Example programs
- Build instructions
- Performance characteristics
- Compatibility notes

### Testing
- Library initialization tests
- Completion queue tests
- Channel lifecycle tests
- Server lifecycle tests
- Byte buffer tests
- Time and deadline tests
- Call lifecycle tests

## Future Releases

### Planned Features
- Full HTTP/2 HPACK header compression
- Complete Protobuf serialization integration
- Full TLS/SSL implementation with OpenSSL
- HTTP/2 flow control and window management
- Load balancing support
- Name resolution (DNS)
- Connection pooling
- Keep-alive support
- Compression (gzip, deflate)
- Metadata API enhancements
- Server interceptors
- Client interceptors
- Reflection API
- Health checking protocol
- Tracing and observability
- Performance benchmarks
- gRPC interop test suite
- Additional examples (streaming, bidirectional)
