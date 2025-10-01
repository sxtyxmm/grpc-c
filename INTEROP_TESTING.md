# Interoperability Testing Recommendations

## Purpose

This document provides recommendations for testing interoperability between `grpc-c` and official gRPC implementations to ensure cross-language compatibility.

## Testing Strategy

### Phase 1: Protocol-Level Validation

#### 1.1 Wire Format Testing
Test that grpc-c sends and receives data in the correct gRPC wire format:

```bash
# Use Wireshark or tcpdump to capture HTTP/2 frames
tcpdump -i lo -w grpc-capture.pcap port 50051

# Verify:
- HTTP/2 frame structure
- HPACK header compression
- gRPC message framing (length-prefixed messages)
- Proper use of HTTP/2 streams
```

#### 1.2 Status Code Compatibility
Verify that all status codes are mapped correctly:
- Create test server/client pairs that return each status code
- Verify status codes are received correctly across implementations

### Phase 2: Cross-Implementation Testing

#### 2.1 C Client → Other Server
Test grpc-c client connecting to servers in other languages:

**Test with gRPC C++ Server:**
```cpp
// server.cpp
#include <grpcpp/grpcpp.h>
// Implement a simple echo service
// Test with grpc-c client
```

**Test with gRPC Go Server:**
```go
// server.go
package main
import "google.golang.org/grpc"
// Implement a simple echo service
// Test with grpc-c client
```

**Test with gRPC Python Server:**
```python
# server.py
import grpc
from concurrent import futures
# Implement a simple echo service
# Test with grpc-c client
```

#### 2.2 Other Client → C Server
Test clients in other languages connecting to grpc-c server:

**Test with gRPC C++ Client:**
```cpp
// client.cpp
#include <grpcpp/grpcpp.h>
// Connect to grpc-c server
```

**Test with gRPC Go Client:**
```go
// client.go
package main
import "google.golang.org/grpc"
// Connect to grpc-c server
```

**Test with gRPC Python Client:**
```python
# client.py
import grpc
# Connect to grpc-c server
```

### Phase 3: Feature Compatibility Testing

#### 3.1 Unary RPC
- Simple request/response
- With metadata
- With deadlines
- With cancellation

#### 3.2 Server Streaming
- Single request, multiple responses
- With flow control
- With cancellation

#### 3.3 Client Streaming
- Multiple requests, single response
- With flow control
- With cancellation

#### 3.4 Bidirectional Streaming
- Full duplex communication
- With flow control
- With cancellation

#### 3.5 Metadata Testing
- Send/receive metadata
- Binary metadata
- Multiple metadata entries
- Reserved headers

#### 3.6 Error Handling
- All status codes
- Status messages
- Error details (if implemented)

#### 3.7 Compression
- gzip compression
- deflate compression
- Negotiation of compression
- Mixed compression scenarios

#### 3.8 Deadlines and Timeouts
- Client-side deadlines
- Server-side timeout handling
- Deadline propagation

### Phase 4: Performance Testing

#### 4.1 Throughput Testing
Compare throughput with official implementations:
```bash
# Benchmark tool
./benchmark --requests 10000 --concurrent 100
```

#### 4.2 Latency Testing
Measure and compare latency:
- P50, P95, P99 latencies
- Under various loads
- With/without compression

#### 4.3 Memory Usage
Compare memory footprint:
- At rest
- Under load
- With many concurrent connections

### Phase 5: Stress Testing

#### 5.1 Connection Limits
Test with many concurrent connections

#### 5.2 Large Messages
Test with large request/response bodies

#### 5.3 Long-Running Streams
Test streaming over extended periods

#### 5.4 Error Recovery
Test recovery from various error conditions

## Recommended Test Suite Structure

```
interop-tests/
├── proto/
│   └── test.proto              # Test service definitions
├── cpp/
│   ├── server.cpp              # C++ test server
│   └── client.cpp              # C++ test client
├── go/
│   ├── server.go               # Go test server
│   └── client.go               # Go test client
├── python/
│   ├── server.py               # Python test server
│   └── client.py               # Python test client
├── c/
│   ├── test_client.c           # grpc-c test client
│   └── test_server.c           # grpc-c test server
└── run_interop_tests.sh        # Test runner script
```

## Test Service Definition

```protobuf
syntax = "proto3";

package interop;

service TestService {
  // Unary call
  rpc UnaryCall (SimpleRequest) returns (SimpleResponse);
  
  // Server streaming
  rpc ServerStreamingCall (SimpleRequest) returns (stream SimpleResponse);
  
  // Client streaming
  rpc ClientStreamingCall (stream SimpleRequest) returns (SimpleResponse);
  
  // Bidirectional streaming
  rpc BidiStreamingCall (stream SimpleRequest) returns (stream SimpleResponse);
  
  // Empty call (for testing)
  rpc EmptyCall (Empty) returns (Empty);
}

message SimpleRequest {
  int32 response_size = 1;
  bytes payload = 2;
}

message SimpleResponse {
  bytes payload = 1;
}

message Empty {}
```

## Implementation Steps

### Step 1: Setup Test Environment
```bash
# Install gRPC for various languages
# C++
git clone https://github.com/grpc/grpc.git
cd grpc && git submodule update --init

# Go
go get google.golang.org/grpc

# Python
pip install grpcio grpcio-tools
```

### Step 2: Generate Test Service Code
```bash
# For each language, generate code from proto
protoc --cpp_out=. --grpc_out=. test.proto
protoc --go_out=. --go-grpc_out=. test.proto
python -m grpc_tools.protoc --python_out=. --grpc_python_out=. test.proto
```

### Step 3: Implement Test Servers
Each test server should:
- Listen on a configurable port
- Implement all test service methods
- Log all received requests
- Handle all status codes
- Support metadata echo

### Step 4: Implement Test Clients
Each test client should:
- Connect to configurable endpoint
- Execute all test scenarios
- Verify responses
- Report test results

### Step 5: Create Test Runner
```bash
#!/bin/bash
# run_interop_tests.sh

# Test matrix
SERVERS="cpp go python c"
CLIENTS="cpp go python c"

for server in $SERVERS; do
    # Start server
    start_server_$server
    
    for client in $CLIENTS; do
        echo "Testing $client client → $server server"
        run_client_$client
        check_results
    done
    
    # Stop server
    stop_server_$server
done
```

## Expected Outcomes

### Success Criteria
- ✅ All unary calls succeed
- ✅ All streaming patterns work
- ✅ Metadata is transmitted correctly
- ✅ All status codes are handled correctly
- ✅ Deadlines are respected
- ✅ Compression works correctly
- ✅ Error handling is consistent

### Known Limitations to Document
Based on current grpc-c implementation:
- ⚠️ TLS/SSL not fully integrated (use external proxy)
- ⚠️ Protobuf serialization requires manual handling
- ⚠️ Full streaming implementation in progress

## Continuous Integration

### GitHub Actions Workflow
```yaml
name: Interop Tests

on: [push, pull_request]

jobs:
  interop:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        server: [cpp, go, python, c]
        client: [cpp, go, python, c]
    steps:
      - uses: actions/checkout@v2
      - name: Setup ${{ matrix.server }} server
        run: ./setup-${{ matrix.server }}.sh
      - name: Setup ${{ matrix.client }} client
        run: ./setup-${{ matrix.client }}.sh
      - name: Run interop test
        run: ./run-interop-test.sh ${{ matrix.server }} ${{ matrix.client }}
```

## Reporting

### Test Report Format
```
Interoperability Test Report
============================

Client: grpc-c
Server: gRPC Go

Unary Call: ✅ PASS
Server Streaming: ✅ PASS
Client Streaming: ✅ PASS
Bidirectional Streaming: ✅ PASS
Metadata: ✅ PASS
Status Codes: ✅ PASS (17/17)
Compression: ✅ PASS
Deadlines: ✅ PASS

Overall: PASS
Compatibility: 100%
```

## Timeline

### Phase 1 (Week 1-2)
- Setup test environment
- Create test service definition
- Implement basic test servers

### Phase 2 (Week 3-4)
- Implement test clients
- Run initial interop tests
- Document findings

### Phase 3 (Week 5-6)
- Fix identified issues
- Performance testing
- Stress testing

### Phase 4 (Week 7-8)
- CI/CD integration
- Documentation
- Final report

## Conclusion

Interoperability testing is crucial for ensuring grpc-c works correctly with other gRPC implementations. This comprehensive testing plan will validate:

1. **Protocol Compliance**: Wire format matches gRPC specification
2. **Cross-Language Compatibility**: Works with C++, Go, Python implementations
3. **Feature Parity**: All major features work correctly
4. **Performance**: Comparable to official implementations
5. **Reliability**: Handles errors and edge cases correctly

Following this testing plan will provide confidence that grpc-c is a drop-in replacement for gRPC in C applications.

---

**Recommendation**: Start with Phase 1 and 2 (basic protocol and cross-implementation testing) as these provide the most value for validating core functionality.
