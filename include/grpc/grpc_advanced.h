/**
 * @file grpc_advanced.h
 * @brief Advanced features for gRPC-C library
 * 
 * This header provides advanced features including load balancing, name resolution,
 * connection pooling, interceptors, reflection API, and observability.
 */

#ifndef GRPC_ADVANCED_H
#define GRPC_ADVANCED_H

#include "grpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Load Balancing
 * ======================================================================== */

typedef enum {
    GRPC_LB_POLICY_ROUND_ROBIN = 0,
    GRPC_LB_POLICY_PICK_FIRST = 1,
    GRPC_LB_POLICY_WEIGHTED = 2
} grpc_lb_policy_type;

typedef struct grpc_lb_policy grpc_lb_policy;

grpc_lb_policy *grpc_lb_policy_create(grpc_lb_policy_type type);
int grpc_lb_policy_add_address(grpc_lb_policy *policy, const char *address, int weight);
const char *grpc_lb_policy_pick(grpc_lb_policy *policy);
int grpc_lb_policy_mark_unavailable(grpc_lb_policy *policy, const char *address);
int grpc_lb_policy_mark_available(grpc_lb_policy *policy, const char *address);
void grpc_lb_policy_destroy(grpc_lb_policy *policy);

/* ========================================================================
 * Name Resolution
 * ======================================================================== */

typedef enum {
    GRPC_RESOLVER_DNS = 0,
    GRPC_RESOLVER_STATIC = 1,
    GRPC_RESOLVER_CUSTOM = 2
} grpc_resolver_type;

typedef struct grpc_resolved_address grpc_resolved_address;
typedef struct grpc_name_resolver grpc_name_resolver;

grpc_name_resolver *grpc_name_resolver_create(grpc_resolver_type type, const char *target);
int grpc_name_resolver_resolve(grpc_name_resolver *resolver);
grpc_resolved_address *grpc_name_resolver_get_addresses(grpc_name_resolver *resolver);
size_t grpc_name_resolver_get_address_count(grpc_name_resolver *resolver);
int grpc_name_resolver_set_custom_resolver(grpc_name_resolver *resolver,
                                           grpc_resolved_address *(*custom_resolve)(const char *, void *),
                                           void *user_data);
void grpc_name_resolver_destroy(grpc_name_resolver *resolver);

/* ========================================================================
 * Connection Pooling
 * ======================================================================== */

typedef struct grpc_connection_pool grpc_connection_pool;
typedef struct http2_connection http2_connection;

grpc_connection_pool *grpc_connection_pool_create(size_t max_connections, int idle_timeout_ms);
int grpc_connection_pool_set_keepalive(grpc_connection_pool *pool,
                                       int interval_ms,
                                       int timeout_ms,
                                       bool permit_without_calls);
http2_connection *grpc_connection_pool_get(grpc_connection_pool *pool, const char *target);
int grpc_connection_pool_return(grpc_connection_pool *pool, const char *target, http2_connection *connection);
void grpc_connection_pool_cleanup_idle(grpc_connection_pool *pool);
void grpc_connection_pool_destroy(grpc_connection_pool *pool);

/* ========================================================================
 * Interceptors
 * ======================================================================== */

typedef struct grpc_client_interceptor_context grpc_client_interceptor_context;
typedef struct grpc_server_interceptor_context grpc_server_interceptor_context;
typedef struct grpc_client_interceptor_chain grpc_client_interceptor_chain;
typedef struct grpc_server_interceptor_chain grpc_server_interceptor_chain;

typedef int (*grpc_client_interceptor_func)(grpc_client_interceptor_context *ctx);
typedef int (*grpc_server_interceptor_func)(grpc_server_interceptor_context *ctx);

/* Client interceptors */
grpc_client_interceptor_chain *grpc_client_interceptor_chain_create(void);
int grpc_client_interceptor_chain_add(grpc_client_interceptor_chain *chain,
                                      grpc_client_interceptor_func interceptor,
                                      void *user_data);
int grpc_client_interceptor_chain_execute(grpc_client_interceptor_chain *chain,
                                         grpc_call *call,
                                         const char *method,
                                         const char *host,
                                         grpc_metadata_array *initial_metadata,
                                         grpc_byte_buffer *send_message);
void grpc_client_interceptor_chain_destroy(grpc_client_interceptor_chain *chain);

/* Server interceptors */
grpc_server_interceptor_chain *grpc_server_interceptor_chain_create(void);
int grpc_server_interceptor_chain_add(grpc_server_interceptor_chain *chain,
                                      grpc_server_interceptor_func interceptor,
                                      void *user_data);
int grpc_server_interceptor_chain_execute(grpc_server_interceptor_chain *chain,
                                         grpc_call *call,
                                         const char *method,
                                         grpc_metadata_array *initial_metadata,
                                         grpc_byte_buffer *recv_message);
void grpc_server_interceptor_chain_destroy(grpc_server_interceptor_chain *chain);

/* Example interceptors */
int grpc_logging_client_interceptor(grpc_client_interceptor_context *ctx);
int grpc_logging_server_interceptor(grpc_server_interceptor_context *ctx);
int grpc_auth_client_interceptor(grpc_client_interceptor_context *ctx);
int grpc_auth_server_interceptor(grpc_server_interceptor_context *ctx);

/* ========================================================================
 * Reflection API
 * ======================================================================== */

typedef struct grpc_service_descriptor grpc_service_descriptor;
typedef struct grpc_method_descriptor grpc_method_descriptor;
typedef struct grpc_reflection_registry grpc_reflection_registry;

grpc_reflection_registry *grpc_reflection_registry_create(void);
int grpc_reflection_registry_add_service(grpc_reflection_registry *registry,
                                         const char *service_name,
                                         const char *package_name);
int grpc_reflection_registry_add_method(grpc_reflection_registry *registry,
                                       const char *service_name,
                                       const char *method_name,
                                       const char *input_type,
                                       const char *output_type,
                                       bool client_streaming,
                                       bool server_streaming);
grpc_service_descriptor *grpc_reflection_registry_list_services(grpc_reflection_registry *registry);
grpc_service_descriptor *grpc_reflection_registry_get_service(grpc_reflection_registry *registry,
                                                              const char *service_name);
size_t grpc_reflection_registry_get_service_count(grpc_reflection_registry *registry);
void grpc_reflection_registry_destroy(grpc_reflection_registry *registry);

char *grpc_reflection_get_full_service_name(grpc_service_descriptor *service);
char *grpc_reflection_get_full_method_name(grpc_service_descriptor *service,
                                          grpc_method_descriptor *method);

/* ========================================================================
 * Observability - Tracing
 * ======================================================================== */

typedef struct grpc_trace_context grpc_trace_context;
typedef struct grpc_trace_span grpc_trace_span;

grpc_trace_context *grpc_trace_context_create(void);
grpc_trace_span *grpc_trace_start_span(grpc_trace_context *ctx,
                                      const char *operation_name,
                                      const char *parent_span_id);
int grpc_trace_finish_span(grpc_trace_context *ctx, grpc_trace_span *span);
int grpc_trace_span_add_tag(grpc_trace_span *span, const char *key, const char *value);
void grpc_trace_context_set_exporter(grpc_trace_context *ctx,
                                    void (*export_func)(grpc_trace_span *, void *),
                                    void *user_data);
void grpc_trace_context_destroy(grpc_trace_context *ctx);

/* ========================================================================
 * Observability - Metrics
 * ======================================================================== */

typedef enum {
    GRPC_METRIC_COUNTER = 0,
    GRPC_METRIC_GAUGE = 1,
    GRPC_METRIC_HISTOGRAM = 2
} grpc_metric_type;

/* Metric structure (exposed for read access) */
typedef struct grpc_metric {
    char *name;
    char *description;
    grpc_metric_type type;
    double value;
    size_t count;
    double sum;
    double min;
    double max;
    struct grpc_metric *next;
} grpc_metric;

typedef struct grpc_metrics_registry grpc_metrics_registry;

grpc_metrics_registry *grpc_metrics_registry_create(void);
int grpc_metrics_register(grpc_metrics_registry *registry,
                          const char *name,
                          const char *description,
                          grpc_metric_type type);
int grpc_metrics_increment(grpc_metrics_registry *registry, const char *name, double value);
int grpc_metrics_set(grpc_metrics_registry *registry, const char *name, double value);
grpc_metric *grpc_metrics_get(grpc_metrics_registry *registry, const char *name);
void grpc_metrics_registry_destroy(grpc_metrics_registry *registry);

/* ========================================================================
 * Observability - Logging
 * ======================================================================== */

typedef enum {
    GRPC_LOG_LEVEL_DEBUG = 0,
    GRPC_LOG_LEVEL_INFO = 1,
    GRPC_LOG_LEVEL_WARNING = 2,
    GRPC_LOG_LEVEL_ERROR = 3
} grpc_log_level;

typedef struct grpc_logger grpc_logger;

grpc_logger *grpc_logger_create(grpc_log_level min_level);
void grpc_logger_set_handler(grpc_logger *logger,
                             void (*log_func)(grpc_log_level, const char *, void *),
                             void *user_data);
void grpc_logger_log(grpc_logger *logger, grpc_log_level level, const char *message);
void grpc_logger_destroy(grpc_logger *logger);

#ifdef __cplusplus
}
#endif

#endif /* GRPC_ADVANCED_H */
