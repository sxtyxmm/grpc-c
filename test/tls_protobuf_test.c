/**
 * @file tls_protobuf_test.c
 * @brief Tests for TLS/SSL and Protobuf integration features
 */

#define _POSIX_C_SOURCE 200809L
#include <grpc/grpc.h>
#include <grpc/grpc_protobuf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test macros */
#define TEST(name) printf("Running test: %s... ", #name)
#define TEST_PASS() printf("PASS\n"); passed++
#define TEST_FAIL(msg) printf("FAIL: %s\n", msg); failed++

static int passed = 0;
static int failed = 0;

/* Test SSL credentials creation */
void test_ssl_credentials_create(void) {
    TEST(test_ssl_credentials_create);
    
    grpc_init();
    
    /* Create credentials with NULL root certs (should use system defaults) */
    grpc_channel_credentials *creds = grpc_ssl_credentials_create(NULL, NULL);
    if (!creds) {
        TEST_FAIL("Failed to create SSL credentials");
        grpc_shutdown();
        return;
    }
    
    grpc_channel_credentials_release(creds);
    
    /* Create credentials with root certs */
    const char *root_cert = "-----BEGIN CERTIFICATE-----\ntest\n-----END CERTIFICATE-----";
    creds = grpc_ssl_credentials_create(root_cert, NULL);
    if (!creds) {
        TEST_FAIL("Failed to create SSL credentials with root cert");
        grpc_shutdown();
        return;
    }
    
    grpc_channel_credentials_release(creds);
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test SSL server credentials creation */
void test_ssl_server_credentials_create(void) {
    TEST(test_ssl_server_credentials_create);
    
    grpc_init();
    
    grpc_ssl_pem_key_cert_pair pair;
    pair.private_key = "-----BEGIN PRIVATE KEY-----\ntest\n-----END PRIVATE KEY-----";
    pair.cert_chain = "-----BEGIN CERTIFICATE-----\ntest\n-----END CERTIFICATE-----";
    
    grpc_server_credentials *creds = grpc_ssl_server_credentials_create(
        NULL, &pair, 1);
    
    if (!creds) {
        TEST_FAIL("Failed to create SSL server credentials");
        grpc_shutdown();
        return;
    }
    
    grpc_server_credentials_release(creds);
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test secure channel creation (framework test - won't actually connect) */
void test_secure_channel_create(void) {
    TEST(test_secure_channel_create);
    
    grpc_init();
    
    /* Create credentials */
    grpc_channel_credentials *creds = grpc_ssl_credentials_create(NULL, NULL);
    if (!creds) {
        TEST_FAIL("Failed to create credentials");
        grpc_shutdown();
        return;
    }
    
    /* Create secure channel (won't connect, just tests API) */
    grpc_channel *channel = grpc_channel_create("localhost:50051", creds, NULL);
    if (!channel) {
        TEST_FAIL("Failed to create secure channel");
        grpc_channel_credentials_release(creds);
        grpc_shutdown();
        return;
    }
    
    /* Cleanup */
    grpc_channel_destroy(channel);
    grpc_channel_credentials_release(creds);
    grpc_shutdown();
    
    TEST_PASS();
}

/* Test protobuf byte buffer creation */
void test_protobuf_buffer_create(void) {
    TEST(test_protobuf_buffer_create);
    
    grpc_init();
    
    const char *test_data = "test protobuf data";
    grpc_byte_buffer *buffer = grpc_protobuf_buffer_create(
        (const uint8_t *)test_data, strlen(test_data));
    
    if (!buffer) {
        TEST_FAIL("Failed to create protobuf buffer");
        grpc_shutdown();
        return;
    }
    
    if (buffer->length != strlen(test_data)) {
        TEST_FAIL("Buffer length mismatch");
        grpc_byte_buffer_destroy(buffer);
        grpc_shutdown();
        return;
    }
    
    grpc_byte_buffer_destroy(buffer);
    grpc_shutdown();
    
    TEST_PASS();
}

int main(void) {
    printf("=== gRPC-C TLS and Protobuf Tests ===\n\n");
    
    /* TLS tests */
    test_ssl_credentials_create();
    test_ssl_server_credentials_create();
    test_secure_channel_create();
    
    /* Protobuf tests */
    test_protobuf_buffer_create();
    
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
