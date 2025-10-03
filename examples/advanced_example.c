/**
 * @file advanced_example.c
 * @brief Example demonstrating advanced gRPC-C features
 * 
 * This example shows:
 * - Load balancing with round-robin
 * - Name resolution
 * - Connection pooling with keep-alive
 * - Client/server interceptors
 * - Reflection API
 * - Tracing and metrics
 * - Enhanced logging
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc/grpc_advanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ========================================================================
 * Custom Logging Handler
 * ======================================================================== */

void custom_log_handler(grpc_log_level level, const char *message, void *user_data) {
    (void)user_data;
    
    const char *level_str;
    switch (level) {
        case GRPC_LOG_LEVEL_DEBUG:   level_str = "DEBUG"; break;
        case GRPC_LOG_LEVEL_INFO:    level_str = "INFO"; break;
        case GRPC_LOG_LEVEL_WARNING: level_str = "WARNING"; break;
        case GRPC_LOG_LEVEL_ERROR:   level_str = "ERROR"; break;
        default:                     level_str = "UNKNOWN"; break;
    }
    
    printf("[%s] %s\n", level_str, message);
}

/* ========================================================================
 * Custom Trace Exporter
 * ======================================================================== */

void custom_trace_exporter(grpc_trace_span *span, void *user_data) {
    (void)user_data;
    (void)span;
    
    printf("Trace span exported (placeholder)\n");
}

/* ========================================================================
 * Load Balancing Example
 * ======================================================================== */

void demonstrate_load_balancing(void) {
    printf("\n=== Load Balancing Example ===\n");
    
    /* Create round-robin load balancer */
    grpc_lb_policy *policy = grpc_lb_policy_create(GRPC_LB_POLICY_ROUND_ROBIN);
    if (!policy) {
        printf("Failed to create load balancing policy\n");
        return;
    }
    
    /* Add backend addresses */
    grpc_lb_policy_add_address(policy, "localhost:50051", 1);
    grpc_lb_policy_add_address(policy, "localhost:50052", 1);
    grpc_lb_policy_add_address(policy, "localhost:50053", 1);
    
    printf("Added 3 backend addresses\n");
    
    /* Demonstrate round-robin selection */
    printf("Round-robin picks:\n");
    for (int i = 0; i < 6; i++) {
        const char *addr = grpc_lb_policy_pick(policy);
        printf("  Pick %d: %s\n", i + 1, addr);
    }
    
    /* Mark one address as unavailable */
    grpc_lb_policy_mark_unavailable(policy, "localhost:50052");
    printf("\nMarked localhost:50052 as unavailable\n");
    printf("Subsequent picks:\n");
    for (int i = 0; i < 4; i++) {
        const char *addr = grpc_lb_policy_pick(policy);
        printf("  Pick %d: %s\n", i + 1, addr);
    }
    
    grpc_lb_policy_destroy(policy);
}

/* ========================================================================
 * Name Resolution Example
 * ======================================================================== */

void demonstrate_name_resolution(void) {
    printf("\n=== Name Resolution Example ===\n");
    
    /* Static resolver */
    printf("Static resolver:\n");
    grpc_name_resolver *static_resolver = grpc_name_resolver_create(GRPC_RESOLVER_STATIC, "127.0.0.1:50051");
    if (static_resolver) {
        if (grpc_name_resolver_resolve(static_resolver) == 0) {
            size_t count = grpc_name_resolver_get_address_count(static_resolver);
            printf("  Resolved %zu address(es)\n", count);
        }
        grpc_name_resolver_destroy(static_resolver);
    }
    
    /* DNS resolver */
    printf("\nDNS resolver:\n");
    grpc_name_resolver *dns_resolver = grpc_name_resolver_create(GRPC_RESOLVER_DNS, "localhost:50051");
    if (dns_resolver) {
        if (grpc_name_resolver_resolve(dns_resolver) == 0) {
            size_t count = grpc_name_resolver_get_address_count(dns_resolver);
            printf("  Resolved %zu address(es)\n", count);
        } else {
            printf("  DNS resolution failed (may be expected in some environments)\n");
        }
        grpc_name_resolver_destroy(dns_resolver);
    }
}

/* ========================================================================
 * Connection Pool Example
 * ======================================================================== */

void demonstrate_connection_pool(void) {
    printf("\n=== Connection Pool Example ===\n");
    
    /* Create connection pool */
    grpc_connection_pool *pool = grpc_connection_pool_create(10, 30000);
    if (!pool) {
        printf("Failed to create connection pool\n");
        return;
    }
    
    printf("Created connection pool (max 10 connections, 30s idle timeout)\n");
    
    /* Configure keep-alive */
    grpc_connection_pool_set_keepalive(pool, 10000, 5000, true);
    printf("Configured keep-alive (10s interval, 5s timeout)\n");
    
    /* Simulate getting connections */
    printf("\nSimulating connection management:\n");
    printf("  Connection pooling allows reusing existing connections\n");
    printf("  Keep-alive maintains healthy connections\n");
    printf("  Idle connections are cleaned up after timeout\n");
    
    grpc_connection_pool_destroy(pool);
}

/* ========================================================================
 * Interceptors Example
 * ======================================================================== */

void demonstrate_interceptors(void) {
    printf("\n=== Interceptors Example ===\n");
    
    /* Client interceptor chain */
    printf("Client interceptor chain:\n");
    grpc_client_interceptor_chain *client_chain = grpc_client_interceptor_chain_create();
    if (client_chain) {
        grpc_client_interceptor_chain_add(client_chain, grpc_logging_client_interceptor, NULL);
        grpc_client_interceptor_chain_add(client_chain, grpc_auth_client_interceptor, NULL);
        printf("  Added logging and authentication interceptors\n");
        grpc_client_interceptor_chain_destroy(client_chain);
    }
    
    /* Server interceptor chain */
    printf("\nServer interceptor chain:\n");
    grpc_server_interceptor_chain *server_chain = grpc_server_interceptor_chain_create();
    if (server_chain) {
        grpc_server_interceptor_chain_add(server_chain, grpc_logging_server_interceptor, NULL);
        grpc_server_interceptor_chain_add(server_chain, grpc_auth_server_interceptor, NULL);
        printf("  Added logging and authentication interceptors\n");
        grpc_server_interceptor_chain_destroy(server_chain);
    }
}

/* ========================================================================
 * Reflection API Example
 * ======================================================================== */

void demonstrate_reflection(void) {
    printf("\n=== Reflection API Example ===\n");
    
    /* Create reflection registry */
    grpc_reflection_registry *registry = grpc_reflection_registry_create();
    if (!registry) {
        printf("Failed to create reflection registry\n");
        return;
    }
    
    /* Add services and methods */
    grpc_reflection_registry_add_service(registry, "GreeterService", "helloworld");
    grpc_reflection_registry_add_method(registry, "GreeterService", "SayHello",
                                       "HelloRequest", "HelloResponse", false, false);
    grpc_reflection_registry_add_method(registry, "GreeterService", "SayHelloStream",
                                       "HelloRequest", "HelloResponse", false, true);
    
    printf("Registered services:\n");
    size_t count = grpc_reflection_registry_get_service_count(registry);
    printf("  Service count: %zu\n", count);
    
    /* Get service details */
    grpc_service_descriptor *service = grpc_reflection_registry_get_service(registry, "GreeterService");
    if (service) {
        char *full_name = grpc_reflection_get_full_service_name(service);
        printf("  Full service name: %s\n", full_name);
        free(full_name);
    }
    
    grpc_reflection_registry_destroy(registry);
}

/* ========================================================================
 * Tracing Example
 * ======================================================================== */

void demonstrate_tracing(void) {
    printf("\n=== Tracing Example ===\n");
    
    /* Create trace context */
    grpc_trace_context *ctx = grpc_trace_context_create();
    if (!ctx) {
        printf("Failed to create trace context\n");
        return;
    }
    
    /* Set custom exporter */
    grpc_trace_context_set_exporter(ctx, custom_trace_exporter, NULL);
    printf("Configured custom trace exporter\n");
    
    /* Start a span */
    grpc_trace_span *span = grpc_trace_start_span(ctx, "example_operation", NULL);
    if (span) {
        printf("Started trace span: example_operation\n");
        
        /* Add tags */
        grpc_trace_span_add_tag(span, "service", "example");
        grpc_trace_span_add_tag(span, "method", "demonstrate_tracing");
        printf("  Added tags to span\n");
        
        /* Simulate work */
        usleep(10000); /* 10ms */
        
        /* Finish span */
        grpc_trace_finish_span(ctx, span);
        printf("  Finished trace span\n");
    }
    
    grpc_trace_context_destroy(ctx);
}

/* ========================================================================
 * Metrics Example
 * ======================================================================== */

void demonstrate_metrics(void) {
    printf("\n=== Metrics Example ===\n");
    
    /* Create metrics registry */
    grpc_metrics_registry *registry = grpc_metrics_registry_create();
    if (!registry) {
        printf("Failed to create metrics registry\n");
        return;
    }
    
    /* Register metrics */
    grpc_metrics_register(registry, "requests_total", "Total requests", GRPC_METRIC_COUNTER);
    grpc_metrics_register(registry, "active_connections", "Active connections", GRPC_METRIC_GAUGE);
    grpc_metrics_register(registry, "request_duration_ms", "Request duration", GRPC_METRIC_HISTOGRAM);
    
    printf("Registered metrics:\n");
    printf("  - requests_total (counter)\n");
    printf("  - active_connections (gauge)\n");
    printf("  - request_duration_ms (histogram)\n");
    
    /* Update metrics */
    grpc_metrics_increment(registry, "requests_total", 1);
    grpc_metrics_increment(registry, "requests_total", 1);
    grpc_metrics_set(registry, "active_connections", 5);
    grpc_metrics_increment(registry, "request_duration_ms", 45.3);
    grpc_metrics_increment(registry, "request_duration_ms", 52.1);
    
    printf("\nMetric values:\n");
    grpc_metric *metric = grpc_metrics_get(registry, "requests_total");
    if (metric) {
        printf("  requests_total: %.0f\n", metric->value);
    }
    
    metric = grpc_metrics_get(registry, "active_connections");
    if (metric) {
        printf("  active_connections: %.0f\n", metric->value);
    }
    
    grpc_metrics_registry_destroy(registry);
}

/* ========================================================================
 * Logging Example
 * ======================================================================== */

void demonstrate_logging(void) {
    printf("\n=== Logging Example ===\n");
    
    /* Create logger */
    grpc_logger *logger = grpc_logger_create(GRPC_LOG_LEVEL_DEBUG);
    if (!logger) {
        printf("Failed to create logger\n");
        return;
    }
    
    /* Set custom log handler */
    grpc_logger_set_handler(logger, custom_log_handler, NULL);
    printf("Configured custom log handler\n\n");
    
    /* Log messages at different levels */
    grpc_logger_log(logger, GRPC_LOG_LEVEL_DEBUG, "This is a debug message");
    grpc_logger_log(logger, GRPC_LOG_LEVEL_INFO, "This is an info message");
    grpc_logger_log(logger, GRPC_LOG_LEVEL_WARNING, "This is a warning message");
    grpc_logger_log(logger, GRPC_LOG_LEVEL_ERROR, "This is an error message");
    
    grpc_logger_destroy(logger);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    printf("========================================\n");
    printf("gRPC-C Advanced Features Example\n");
    printf("========================================\n");
    
    /* Initialize gRPC */
    grpc_init();
    
    /* Demonstrate each feature */
    demonstrate_load_balancing();
    demonstrate_name_resolution();
    demonstrate_connection_pool();
    demonstrate_interceptors();
    demonstrate_reflection();
    demonstrate_tracing();
    demonstrate_metrics();
    demonstrate_logging();
    
    /* Cleanup */
    grpc_shutdown();
    
    printf("\n========================================\n");
    printf("All demonstrations completed successfully!\n");
    printf("========================================\n");
    
    return 0;
}
