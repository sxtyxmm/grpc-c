/**
 * @file enhanced_test.c
 * @brief Test suite for enhanced gRPC-C features
 */

#include "grpc/grpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("Running test: %s... ", #name)
#define TEST_PASS() printf("PASS\n")
#define TEST_FAIL(msg) do { printf("FAIL: %s\n", msg); exit(1); } while(0)

/* Test metadata array operations */
void test_metadata_array(void) {
    TEST(test_metadata_array);
    
    grpc_init();
    
    grpc_metadata_array array;
    if (grpc_metadata_array_init(&array, 8) != 0) {
        TEST_FAIL("Failed to initialize metadata array");
    }
    
    /* Add some metadata */
    if (grpc_metadata_array_add(&array, "content-type", "application/grpc", 16) != 0) {
        TEST_FAIL("Failed to add metadata");
    }
    
    if (grpc_metadata_array_add(&array, "user-agent", "grpc-c/1.0", 10) != 0) {
        TEST_FAIL("Failed to add second metadata");
    }
    
    if (array.count != 2) {
        TEST_FAIL("Metadata count incorrect");
    }
    
    if (strcmp(array.metadata[0].key, "content-type") != 0) {
        TEST_FAIL("First metadata key incorrect");
    }
    
    if (strcmp(array.metadata[1].key, "user-agent") != 0) {
        TEST_FAIL("Second metadata key incorrect");
    }
    
    /* Cleanup */
    grpc_metadata_array_destroy(&array);
    
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test compression/decompression */
void test_compression(void) {
    TEST(test_compression);
    
    grpc_init();
    
    const char *test_data = "Hello, gRPC! This is a test message for compression.";
    size_t test_len = strlen(test_data);
    
    /* Test gzip compression */
    uint8_t *compressed = NULL;
    size_t compressed_len = 0;
    
    if (grpc_compress((const uint8_t *)test_data, test_len, &compressed, &compressed_len, "gzip") != 0) {
        TEST_FAIL("Failed to compress data");
    }
    
    if (compressed == NULL || compressed_len == 0) {
        TEST_FAIL("Compression produced no output");
    }
    
    /* Decompress */
    uint8_t *decompressed = NULL;
    size_t decompressed_len = 0;
    
    if (grpc_decompress(compressed, compressed_len, &decompressed, &decompressed_len, "gzip") != 0) {
        free(compressed);
        TEST_FAIL("Failed to decompress data");
    }
    
    if (decompressed_len != test_len) {
        free(compressed);
        free(decompressed);
        TEST_FAIL("Decompressed length doesn't match original");
    }
    
    if (memcmp(test_data, decompressed, test_len) != 0) {
        free(compressed);
        free(decompressed);
        TEST_FAIL("Decompressed data doesn't match original");
    }
    
    free(compressed);
    free(decompressed);
    
    /* Test identity (no compression) */
    compressed = NULL;
    compressed_len = 0;
    
    if (grpc_compress((const uint8_t *)test_data, test_len, &compressed, &compressed_len, "identity") != 0) {
        TEST_FAIL("Failed to 'compress' with identity");
    }
    
    if (compressed_len != test_len || memcmp(test_data, compressed, test_len) != 0) {
        free(compressed);
        TEST_FAIL("Identity compression modified data");
    }
    
    free(compressed);
    
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test HPACK integer encoding/decoding */
extern int hpack_encode_integer(uint32_t value, uint8_t prefix_bits, uint8_t *output, size_t output_len);
extern int hpack_decode_integer(const uint8_t *input, size_t input_len, uint8_t prefix_bits, uint32_t *value);

void test_hpack_integer(void) {
    TEST(test_hpack_integer);
    
    grpc_init();
    
    /* Test encoding small value */
    uint8_t buffer[10];
    int bytes = hpack_encode_integer(10, 5, buffer, sizeof(buffer));
    if (bytes != 1 || buffer[0] != 10) {
        TEST_FAIL("Failed to encode small integer");
    }
    
    /* Test decoding small value */
    uint32_t decoded;
    bytes = hpack_decode_integer(buffer, sizeof(buffer), 5, &decoded);
    if (bytes != 1 || decoded != 10) {
        TEST_FAIL("Failed to decode small integer");
    }
    
    /* Test encoding larger value */
    bytes = hpack_encode_integer(1337, 5, buffer, sizeof(buffer));
    if (bytes < 2) {
        TEST_FAIL("Failed to encode large integer");
    }
    
    /* Test decoding larger value */
    bytes = hpack_decode_integer(buffer, sizeof(buffer), 5, &decoded);
    if (decoded != 1337) {
        TEST_FAIL("Failed to decode large integer");
    }
    
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test streaming call creation */
void test_streaming_calls(void) {
    TEST(test_streaming_calls);
    
    grpc_init();
    
    grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
    if (!channel) {
        TEST_FAIL("Failed to create channel");
    }
    
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    if (!cq) {
        grpc_channel_destroy(channel);
        TEST_FAIL("Failed to create completion queue");
    }
    
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(5000);
    
    /* Test server streaming call */
    grpc_call *server_stream = grpc_channel_create_server_streaming_call(
        channel, cq, "/test/ServerStream", NULL, deadline);
    if (!server_stream) {
        grpc_completion_queue_destroy(cq);
        grpc_channel_destroy(channel);
        TEST_FAIL("Failed to create server streaming call");
    }
    grpc_call_destroy(server_stream);
    
    /* Test client streaming call */
    grpc_call *client_stream = grpc_channel_create_client_streaming_call(
        channel, cq, "/test/ClientStream", NULL, deadline);
    if (!client_stream) {
        grpc_completion_queue_destroy(cq);
        grpc_channel_destroy(channel);
        TEST_FAIL("Failed to create client streaming call");
    }
    grpc_call_destroy(client_stream);
    
    /* Test bidirectional streaming call */
    grpc_call *bidi_stream = grpc_channel_create_bidi_streaming_call(
        channel, cq, "/test/BidiStream", NULL, deadline);
    if (!bidi_stream) {
        grpc_completion_queue_destroy(cq);
        grpc_channel_destroy(channel);
        TEST_FAIL("Failed to create bidirectional streaming call");
    }
    grpc_call_destroy(bidi_stream);
    
    grpc_completion_queue_destroy(cq);
    grpc_channel_destroy(channel);
    
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test health check */
void test_health_check(void) {
    TEST(test_health_check);
    
    grpc_init();
    
    grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
    if (!channel) {
        TEST_FAIL("Failed to create channel");
    }
    
    /* Note: This will fail if no server is running, but it tests the API */
    int result = grpc_health_check(channel, "");
    /* We only check that the function doesn't crash */
    (void)result;
    
    grpc_channel_destroy(channel);
    
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test flow control initialization */
extern void http2_flow_control_init_connection(void *conn);
extern void http2_flow_control_init_stream(void *stream);

void test_flow_control_init(void) {
    TEST(test_flow_control_init);
    
    grpc_init();
    
    /* Just test that these functions don't crash with NULL */
    http2_flow_control_init_connection(NULL);
    http2_flow_control_init_stream(NULL);
    
    grpc_shutdown();
    
    TEST_PASS();
}

int main(void) {
    int passed = 0;
    int failed = 0;
    
    printf("=== gRPC-C Enhanced Features Tests ===\n\n");
    
    test_metadata_array();
    passed++;
    
    test_compression();
    passed++;
    
    test_hpack_integer();
    passed++;
    
    test_streaming_calls();
    passed++;
    
    test_health_check();
    passed++;
    
    test_flow_control_init();
    passed++;
    
    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    
    if (failed == 0) {
        printf("\nAll tests PASSED!\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n");
        return 1;
    }
}
