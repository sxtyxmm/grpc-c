# Changelog

All notable changes to the grpc-c project will be documented in this file.

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
