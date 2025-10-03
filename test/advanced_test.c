/**
 * @file advanced_test.c
 * @brief Test suite for advanced gRPC-C features
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc/grpc_advanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macros */
#define TEST_START(name) \
    printf("Running test: %s... ", name); \
    fflush(stdout);

#define TEST_PASS() \
    printf("PASS\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("FAIL: %s\n", msg); \
    tests_failed++;

/* ========================================================================
 * Load Balancing Tests
 * ======================================================================== */

void test_load_balancing_round_robin(void) {
    TEST_START("test_load_balancing_round_robin");
    
    grpc_lb_policy *policy = grpc_lb_policy_create(GRPC_LB_POLICY_ROUND_ROBIN);
    assert(policy != NULL);
    
    /* Add addresses */
    assert(grpc_lb_policy_add_address(policy, "localhost:50051", 1) == 0);
    assert(grpc_lb_policy_add_address(policy, "localhost:50052", 1) == 0);
    assert(grpc_lb_policy_add_address(policy, "localhost:50053", 1) == 0);
    
    /* Pick addresses in round-robin fashion */
    const char *addr1 = grpc_lb_policy_pick(policy);
    assert(addr1 != NULL);
    
    const char *addr2 = grpc_lb_policy_pick(policy);
    assert(addr2 != NULL);
    assert(strcmp(addr1, addr2) != 0);
    
    const char *addr3 = grpc_lb_policy_pick(policy);
    assert(addr3 != NULL);
    
    /* Should cycle back */
    const char *addr4 = grpc_lb_policy_pick(policy);
    assert(addr4 != NULL);
    assert(strcmp(addr1, addr4) == 0);
    
    grpc_lb_policy_destroy(policy);
    TEST_PASS();
}

void test_load_balancing_pick_first(void) {
    TEST_START("test_load_balancing_pick_first");
    
    grpc_lb_policy *policy = grpc_lb_policy_create(GRPC_LB_POLICY_PICK_FIRST);
    assert(policy != NULL);
    
    /* Add addresses */
    assert(grpc_lb_policy_add_address(policy, "localhost:50051", 1) == 0);
    assert(grpc_lb_policy_add_address(policy, "localhost:50052", 1) == 0);
    
    /* Should always pick first address */
    const char *addr1 = grpc_lb_policy_pick(policy);
    assert(addr1 != NULL);
    assert(strcmp(addr1, "localhost:50051") == 0);
    
    const char *addr2 = grpc_lb_policy_pick(policy);
    assert(addr2 != NULL);
    assert(strcmp(addr2, "localhost:50051") == 0);
    
    grpc_lb_policy_destroy(policy);
    TEST_PASS();
}

void test_load_balancing_weighted(void) {
    TEST_START("test_load_balancing_weighted");
    
    grpc_lb_policy *policy = grpc_lb_policy_create(GRPC_LB_POLICY_WEIGHTED);
    assert(policy != NULL);
    
    /* Add addresses with different weights */
    assert(grpc_lb_policy_add_address(policy, "localhost:50051", 10) == 0);
    assert(grpc_lb_policy_add_address(policy, "localhost:50052", 1) == 0);
    
    /* Pick multiple times and verify we get addresses */
    int count = 0;
    for (int i = 0; i < 10; i++) {
        const char *addr = grpc_lb_policy_pick(policy);
        if (addr != NULL) {
            count++;
        }
    }
    assert(count == 10);
    
    grpc_lb_policy_destroy(policy);
    TEST_PASS();
}

/* ========================================================================
 * Name Resolution Tests
 * ======================================================================== */

void test_name_resolver_static(void) {
    TEST_START("test_name_resolver_static");
    
    grpc_name_resolver *resolver = grpc_name_resolver_create(GRPC_RESOLVER_STATIC, "127.0.0.1:50051");
    assert(resolver != NULL);
    
    /* Resolve address */
    assert(grpc_name_resolver_resolve(resolver) == 0);
    
    /* Get addresses */
    size_t count = grpc_name_resolver_get_address_count(resolver);
    assert(count == 1);
    
    grpc_name_resolver_destroy(resolver);
    TEST_PASS();
}

void test_name_resolver_dns(void) {
    TEST_START("test_name_resolver_dns");
    
    grpc_name_resolver *resolver = grpc_name_resolver_create(GRPC_RESOLVER_DNS, "localhost:50051");
    assert(resolver != NULL);
    
    /* Try to resolve - may succeed or fail depending on DNS availability */
    int result = grpc_name_resolver_resolve(resolver);
    (void)result; /* We don't assert on this as DNS may not be available */
    
    grpc_name_resolver_destroy(resolver);
    TEST_PASS();
}

/* ========================================================================
 * Connection Pool Tests
 * ======================================================================== */

void test_connection_pool_create_destroy(void) {
    TEST_START("test_connection_pool_create_destroy");
    
    grpc_connection_pool *pool = grpc_connection_pool_create(10, 30000);
    assert(pool != NULL);
    
    grpc_connection_pool_destroy(pool);
    TEST_PASS();
}

void test_connection_pool_keepalive_config(void) {
    TEST_START("test_connection_pool_keepalive_config");
    
    grpc_connection_pool *pool = grpc_connection_pool_create(10, 30000);
    assert(pool != NULL);
    
    /* Set keep-alive configuration */
    assert(grpc_connection_pool_set_keepalive(pool, 10000, 5000, true) == 0);
    
    grpc_connection_pool_destroy(pool);
    TEST_PASS();
}

/* ========================================================================
 * Interceptor Tests
 * ======================================================================== */

void test_client_interceptor_chain(void) {
    TEST_START("test_client_interceptor_chain");
    
    grpc_client_interceptor_chain *chain = grpc_client_interceptor_chain_create();
    assert(chain != NULL);
    
    /* Add interceptors */
    assert(grpc_client_interceptor_chain_add(chain, grpc_logging_client_interceptor, NULL) == 0);
    assert(grpc_client_interceptor_chain_add(chain, grpc_auth_client_interceptor, NULL) == 0);
    
    grpc_client_interceptor_chain_destroy(chain);
    TEST_PASS();
}

void test_server_interceptor_chain(void) {
    TEST_START("test_server_interceptor_chain");
    
    grpc_server_interceptor_chain *chain = grpc_server_interceptor_chain_create();
    assert(chain != NULL);
    
    /* Add interceptors */
    assert(grpc_server_interceptor_chain_add(chain, grpc_logging_server_interceptor, NULL) == 0);
    assert(grpc_server_interceptor_chain_add(chain, grpc_auth_server_interceptor, NULL) == 0);
    
    grpc_server_interceptor_chain_destroy(chain);
    TEST_PASS();
}

/* ========================================================================
 * Reflection API Tests
 * ======================================================================== */

void test_reflection_registry(void) {
    TEST_START("test_reflection_registry");
    
    grpc_reflection_registry *registry = grpc_reflection_registry_create();
    assert(registry != NULL);
    
    /* Add service */
    assert(grpc_reflection_registry_add_service(registry, "TestService", "test.package") == 0);
    
    /* Add method */
    assert(grpc_reflection_registry_add_method(registry, "TestService", "TestMethod",
                                               "TestRequest", "TestResponse",
                                               false, false) == 0);
    
    /* Get service count */
    size_t count = grpc_reflection_registry_get_service_count(registry);
    assert(count == 1);
    
    /* Get service */
    grpc_service_descriptor *service = grpc_reflection_registry_get_service(registry, "TestService");
    assert(service != NULL);
    
    grpc_reflection_registry_destroy(registry);
    TEST_PASS();
}

/* ========================================================================
 * Tracing Tests
 * ======================================================================== */

void test_trace_context(void) {
    TEST_START("test_trace_context");
    
    grpc_trace_context *ctx = grpc_trace_context_create();
    assert(ctx != NULL);
    
    /* Start span */
    grpc_trace_span *span = grpc_trace_start_span(ctx, "test_operation", NULL);
    assert(span != NULL);
    
    /* Add tag */
    assert(grpc_trace_span_add_tag(span, "key", "value") == 0);
    
    /* Finish span */
    assert(grpc_trace_finish_span(ctx, span) == 0);
    
    grpc_trace_context_destroy(ctx);
    TEST_PASS();
}

/* ========================================================================
 * Metrics Tests
 * ======================================================================== */

void test_metrics_registry(void) {
    TEST_START("test_metrics_registry");
    
    grpc_metrics_registry *registry = grpc_metrics_registry_create();
    assert(registry != NULL);
    
    /* Register metrics */
    assert(grpc_metrics_register(registry, "test_counter", "Test counter", GRPC_METRIC_COUNTER) == 0);
    assert(grpc_metrics_register(registry, "test_gauge", "Test gauge", GRPC_METRIC_GAUGE) == 0);
    
    /* Increment counter */
    assert(grpc_metrics_increment(registry, "test_counter", 1.0) == 0);
    assert(grpc_metrics_increment(registry, "test_counter", 2.0) == 0);
    
    /* Set gauge */
    assert(grpc_metrics_set(registry, "test_gauge", 42.0) == 0);
    
    /* Get metric */
    grpc_metric *metric = grpc_metrics_get(registry, "test_counter");
    assert(metric != NULL);
    
    grpc_metrics_registry_destroy(registry);
    TEST_PASS();
}

/* ========================================================================
 * Logger Tests
 * ======================================================================== */

void test_logger(void) {
    TEST_START("test_logger");
    
    grpc_logger *logger = grpc_logger_create(GRPC_LOG_LEVEL_INFO);
    assert(logger != NULL);
    
    /* Log message */
    grpc_logger_log(logger, GRPC_LOG_LEVEL_INFO, "Test message");
    
    grpc_logger_destroy(logger);
    TEST_PASS();
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("=== gRPC-C Advanced Features Tests ===\n\n");
    
    /* Initialize gRPC */
    grpc_init();
    
    /* Load Balancing Tests */
    test_load_balancing_round_robin();
    test_load_balancing_pick_first();
    test_load_balancing_weighted();
    
    /* Name Resolution Tests */
    test_name_resolver_static();
    test_name_resolver_dns();
    
    /* Connection Pool Tests */
    test_connection_pool_create_destroy();
    test_connection_pool_keepalive_config();
    
    /* Interceptor Tests */
    test_client_interceptor_chain();
    test_server_interceptor_chain();
    
    /* Reflection API Tests */
    test_reflection_registry();
    
    /* Tracing Tests */
    test_trace_context();
    
    /* Metrics Tests */
    test_metrics_registry();
    
    /* Logger Tests */
    test_logger();
    
    /* Cleanup */
    grpc_shutdown();
    
    /* Print results */
    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\nAll tests PASSED!\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n");
        return 1;
    }
}
