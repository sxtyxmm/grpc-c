/**
 * @file basic_test.c
 * @brief Basic tests for gRPC-C library
 */

#include "grpc/grpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) \
    do { \
        printf("Running test: %s...", #name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() \
    do { \
        printf(" PASS\n"); \
        tests_passed++; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        printf(" FAIL: %s\n", msg); \
        tests_failed++; \
    } while(0)

static int tests_passed = 0;
static int tests_failed = 0;

void test_version(void) {
    TEST(test_version);
    const char *version = grpc_version_string();
    if (version && strlen(version) > 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("Version string is empty");
    }
}

void test_init_shutdown(void) {
    TEST(test_init_shutdown);
    grpc_init();
    grpc_shutdown();
    TEST_PASS();
}

void test_completion_queue(void) {
    TEST(test_completion_queue);
    
    grpc_init();
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    
    if (!cq) {
        TEST_FAIL("Failed to create completion queue");
        grpc_shutdown();
        return;
    }
    
    grpc_completion_queue_shutdown(cq);
    grpc_completion_queue_destroy(cq);
    grpc_shutdown();
    
    TEST_PASS();
}

void test_insecure_channel_create_destroy(void) {
    TEST(test_insecure_channel_create_destroy);
    
    grpc_init();
    
    /* Note: Creating a channel doesn't immediately connect, so this should succeed
     * even if the server isn't running. The connection happens when calls are made. */
    grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
    
    if (!channel) {
        TEST_FAIL("Failed to create channel");
        grpc_shutdown();
        return;
    }
    
    grpc_channel_destroy(channel);
    grpc_shutdown();
    
    TEST_PASS();
}

void test_server_create_destroy(void) {
    TEST(test_server_create_destroy);
    
    grpc_init();
    grpc_server *server = grpc_server_create(NULL);
    
    if (!server) {
        TEST_FAIL("Failed to create server");
        grpc_shutdown();
        return;
    }
    
    grpc_server_destroy(server);
    grpc_shutdown();
    
    TEST_PASS();
}

void test_server_add_port(void) {
    TEST(test_server_add_port);
    
    grpc_init();
    grpc_server *server = grpc_server_create(NULL);
    
    if (!server) {
        TEST_FAIL("Failed to create server");
        grpc_shutdown();
        return;
    }
    
    int port = grpc_server_add_insecure_http2_port(server, "0.0.0.0:50051");
    
    if (port != 50051) {
        TEST_FAIL("Failed to add port");
        grpc_server_destroy(server);
        grpc_shutdown();
        return;
    }
    
    grpc_server_destroy(server);
    grpc_shutdown();
    
    TEST_PASS();
}

void test_byte_buffer(void) {
    TEST(test_byte_buffer);
    
    grpc_init();
    
    const uint8_t test_data[] = "Hello, gRPC!";
    size_t test_len = sizeof(test_data);
    
    grpc_byte_buffer *buffer = grpc_byte_buffer_create(test_data, test_len);
    
    if (!buffer) {
        TEST_FAIL("Failed to create byte buffer");
        grpc_shutdown();
        return;
    }
    
    if (buffer->length != test_len) {
        TEST_FAIL("Byte buffer length mismatch");
        grpc_byte_buffer_destroy(buffer);
        grpc_shutdown();
        return;
    }
    
    if (memcmp(buffer->data, test_data, test_len) != 0) {
        TEST_FAIL("Byte buffer data mismatch");
        grpc_byte_buffer_destroy(buffer);
        grpc_shutdown();
        return;
    }
    
    grpc_byte_buffer_destroy(buffer);
    grpc_shutdown();
    
    TEST_PASS();
}

void test_timespec(void) {
    TEST(test_timespec);
    
    grpc_init();
    
    grpc_timespec now = grpc_now();
    if (now.tv_sec == 0) {
        TEST_FAIL("grpc_now returned zero");
        grpc_shutdown();
        return;
    }
    
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(1000);
    if (deadline.tv_sec <= now.tv_sec) {
        TEST_FAIL("Deadline should be in the future");
        grpc_shutdown();
        return;
    }
    
    grpc_shutdown();
    TEST_PASS();
}

void test_call_lifecycle(void) {
    TEST(test_call_lifecycle);
    
    grpc_init();
    
    grpc_channel *channel = grpc_insecure_channel_create("localhost:50051", NULL);
    if (!channel) {
        TEST_FAIL("Failed to create channel");
        grpc_shutdown();
        return;
    }
    
    grpc_completion_queue *cq = grpc_completion_queue_create(GRPC_CQ_NEXT);
    if (!cq) {
        TEST_FAIL("Failed to create completion queue");
        grpc_channel_destroy(channel);
        grpc_shutdown();
        return;
    }
    
    grpc_timespec deadline = grpc_timeout_milliseconds_to_deadline(5000);
    grpc_call *call = grpc_channel_create_call(channel, NULL, 0, cq,
                                                 "/test.Service/Method",
                                                 NULL, deadline);
    
    if (!call) {
        TEST_FAIL("Failed to create call");
        grpc_completion_queue_destroy(cq);
        grpc_channel_destroy(channel);
        grpc_shutdown();
        return;
    }
    
    grpc_call_destroy(call);
    grpc_completion_queue_shutdown(cq);
    grpc_completion_queue_destroy(cq);
    grpc_channel_destroy(channel);
    grpc_shutdown();
    
    TEST_PASS();
}

int main(void) {
    printf("=== gRPC-C Basic Tests ===\n\n");
    
    test_version();
    test_init_shutdown();
    test_completion_queue();
    test_insecure_channel_create_destroy();
    test_server_create_destroy();
    test_server_add_port();
    test_byte_buffer();
    test_timespec();
    test_call_lifecycle();
    
    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed > 0) {
        printf("\nSome tests FAILED!\n");
        return 1;
    }
    
    printf("\nAll tests PASSED!\n");
    return 0;
}
