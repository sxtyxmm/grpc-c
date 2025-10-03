/**
 * @file observability.c
 * @brief Tracing, metrics, and enhanced logging for gRPC
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

/* ========================================================================
 * Tracing Types
 * ======================================================================== */

/* Trace span */
typedef struct grpc_trace_span {
    char *trace_id;
    char *span_id;
    char *parent_span_id;
    char *operation_name;
    struct timeval start_time;
    struct timeval end_time;
    bool finished;
    grpc_metadata_array tags;
    grpc_metadata_array logs;
    struct grpc_trace_span *next;
} grpc_trace_span;

/* Trace context */
typedef struct grpc_trace_context {
    grpc_trace_span *active_spans;
    size_t span_count;
    pthread_mutex_t mutex;
    /* Trace exporter callback */
    void (*export_span)(grpc_trace_span *span, void *user_data);
    void *exporter_user_data;
} grpc_trace_context;

/* ========================================================================
 * Metrics Types
 * ======================================================================== */

/* Metric type */
typedef enum {
    GRPC_METRIC_COUNTER,
    GRPC_METRIC_GAUGE,
    GRPC_METRIC_HISTOGRAM
} grpc_metric_type;

/* Metric */
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

/* Metrics registry */
typedef struct grpc_metrics_registry {
    grpc_metric *metrics;
    size_t metric_count;
    pthread_mutex_t mutex;
} grpc_metrics_registry;

/* ========================================================================
 * Logging Types
 * ======================================================================== */

typedef enum {
    GRPC_LOG_LEVEL_DEBUG,
    GRPC_LOG_LEVEL_INFO,
    GRPC_LOG_LEVEL_WARNING,
    GRPC_LOG_LEVEL_ERROR
} grpc_log_level;

typedef struct grpc_logger {
    grpc_log_level min_level;
    void (*log_func)(grpc_log_level level, const char *message, void *user_data);
    void *user_data;
    pthread_mutex_t mutex;
} grpc_logger;

/* ========================================================================
 * Trace Span Management
 * ======================================================================== */

static char *grpc_generate_id(void) {
    static const char charset[] = "0123456789abcdef";
    char *id = (char *)malloc(17); /* 16 hex chars + null */
    if (!id) {
        return NULL;
    }
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand((unsigned int)(tv.tv_sec * 1000000 + tv.tv_usec));
    
    for (int i = 0; i < 16; i++) {
        id[i] = charset[rand() % 16];
    }
    id[16] = '\0';
    
    return id;
}

static grpc_trace_span *grpc_trace_span_create(const char *operation_name,
                                               const char *parent_span_id) {
    grpc_trace_span *span = (grpc_trace_span *)calloc(1, sizeof(grpc_trace_span));
    if (!span) {
        return NULL;
    }
    
    span->trace_id = grpc_generate_id();
    span->span_id = grpc_generate_id();
    
    if (!span->trace_id || !span->span_id) {
        free(span->trace_id);
        free(span->span_id);
        free(span);
        return NULL;
    }
    
    if (parent_span_id) {
        span->parent_span_id = strdup(parent_span_id);
    } else {
        span->parent_span_id = NULL;
    }
    
    span->operation_name = strdup(operation_name);
    if (!span->operation_name) {
        free(span->trace_id);
        free(span->span_id);
        free(span->parent_span_id);
        free(span);
        return NULL;
    }
    
    gettimeofday(&span->start_time, NULL);
    span->finished = false;
    span->tags.count = 0;
    span->tags.capacity = 0;
    span->tags.metadata = NULL;
    span->logs.count = 0;
    span->logs.capacity = 0;
    span->logs.metadata = NULL;
    span->next = NULL;
    
    return span;
}

static void grpc_trace_span_destroy(grpc_trace_span *span) {
    if (!span) return;
    
    free(span->trace_id);
    free(span->span_id);
    free(span->parent_span_id);
    free(span->operation_name);
    
    if (span->tags.metadata) {
        for (size_t i = 0; i < span->tags.count; i++) {
            free((void *)span->tags.metadata[i].key);
            free((void *)span->tags.metadata[i].value);
        }
        free(span->tags.metadata);
    }
    
    if (span->logs.metadata) {
        for (size_t i = 0; i < span->logs.count; i++) {
            free((void *)span->logs.metadata[i].key);
            free((void *)span->logs.metadata[i].value);
        }
        free(span->logs.metadata);
    }
    
    free(span);
}

/* ========================================================================
 * Trace Context API
 * ======================================================================== */

grpc_trace_context *grpc_trace_context_create(void) {
    grpc_trace_context *ctx = (grpc_trace_context *)calloc(1, sizeof(grpc_trace_context));
    if (!ctx) {
        return NULL;
    }
    
    ctx->active_spans = NULL;
    ctx->span_count = 0;
    ctx->export_span = NULL;
    ctx->exporter_user_data = NULL;
    pthread_mutex_init(&ctx->mutex, NULL);
    
    return ctx;
}

grpc_trace_span *grpc_trace_start_span(grpc_trace_context *ctx,
                                      const char *operation_name,
                                      const char *parent_span_id) {
    if (!ctx || !operation_name) {
        return NULL;
    }
    
    grpc_trace_span *span = grpc_trace_span_create(operation_name, parent_span_id);
    if (!span) {
        return NULL;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    span->next = ctx->active_spans;
    ctx->active_spans = span;
    ctx->span_count++;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return span;
}

int grpc_trace_finish_span(grpc_trace_context *ctx, grpc_trace_span *span) {
    if (!ctx || !span) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    gettimeofday(&span->end_time, NULL);
    span->finished = true;
    
    /* Export span if exporter is configured */
    if (ctx->export_span) {
        ctx->export_span(span, ctx->exporter_user_data);
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return 0;
}

int grpc_trace_span_add_tag(grpc_trace_span *span, const char *key, const char *value) {
    if (!span || !key || !value) {
        return -1;
    }
    
    /* Allocate or resize tags array */
    if (span->tags.count >= span->tags.capacity) {
        size_t new_capacity = span->tags.capacity == 0 ? 4 : span->tags.capacity * 2;
        grpc_metadata *new_tags = (grpc_metadata *)realloc(span->tags.metadata,
                                                           new_capacity * sizeof(grpc_metadata));
        if (!new_tags) {
            return -1;
        }
        span->tags.metadata = new_tags;
        span->tags.capacity = new_capacity;
    }
    
    span->tags.metadata[span->tags.count].key = strdup(key);
    span->tags.metadata[span->tags.count].value = strdup(value);
    span->tags.metadata[span->tags.count].value_length = strlen(value);
    span->tags.count++;
    
    return 0;
}

void grpc_trace_context_set_exporter(grpc_trace_context *ctx,
                                    void (*export_func)(grpc_trace_span *, void *),
                                    void *user_data) {
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->mutex);
    ctx->export_span = export_func;
    ctx->exporter_user_data = user_data;
    pthread_mutex_unlock(&ctx->mutex);
}

void grpc_trace_context_destroy(grpc_trace_context *ctx) {
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->mutex);
    
    grpc_trace_span *span = ctx->active_spans;
    while (span) {
        grpc_trace_span *next = span->next;
        grpc_trace_span_destroy(span);
        span = next;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    pthread_mutex_destroy(&ctx->mutex);
    
    free(ctx);
}

/* ========================================================================
 * Metrics Registry API
 * ======================================================================== */

grpc_metrics_registry *grpc_metrics_registry_create(void) {
    grpc_metrics_registry *registry = (grpc_metrics_registry *)calloc(1, sizeof(grpc_metrics_registry));
    if (!registry) {
        return NULL;
    }
    
    registry->metrics = NULL;
    registry->metric_count = 0;
    pthread_mutex_init(&registry->mutex, NULL);
    
    return registry;
}

int grpc_metrics_register(grpc_metrics_registry *registry,
                          const char *name,
                          const char *description,
                          grpc_metric_type type) {
    if (!registry || !name) {
        return -1;
    }
    
    grpc_metric *metric = (grpc_metric *)calloc(1, sizeof(grpc_metric));
    if (!metric) {
        return -1;
    }
    
    metric->name = strdup(name);
    metric->description = description ? strdup(description) : NULL;
    metric->type = type;
    metric->value = 0;
    metric->count = 0;
    metric->sum = 0;
    metric->min = 0;
    metric->max = 0;
    metric->next = NULL;
    
    pthread_mutex_lock(&registry->mutex);
    
    metric->next = registry->metrics;
    registry->metrics = metric;
    registry->metric_count++;
    
    pthread_mutex_unlock(&registry->mutex);
    
    return 0;
}

int grpc_metrics_increment(grpc_metrics_registry *registry, const char *name, double value) {
    if (!registry || !name) {
        return -1;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    grpc_metric *metric = registry->metrics;
    while (metric) {
        if (strcmp(metric->name, name) == 0) {
            metric->value += value;
            metric->count++;
            metric->sum += value;
            
            if (metric->count == 1 || value < metric->min) {
                metric->min = value;
            }
            if (metric->count == 1 || value > metric->max) {
                metric->max = value;
            }
            
            pthread_mutex_unlock(&registry->mutex);
            return 0;
        }
        metric = metric->next;
    }
    
    pthread_mutex_unlock(&registry->mutex);
    return -1;
}

int grpc_metrics_set(grpc_metrics_registry *registry, const char *name, double value) {
    if (!registry || !name) {
        return -1;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    grpc_metric *metric = registry->metrics;
    while (metric) {
        if (strcmp(metric->name, name) == 0) {
            metric->value = value;
            pthread_mutex_unlock(&registry->mutex);
            return 0;
        }
        metric = metric->next;
    }
    
    pthread_mutex_unlock(&registry->mutex);
    return -1;
}

grpc_metric *grpc_metrics_get(grpc_metrics_registry *registry, const char *name) {
    if (!registry || !name) {
        return NULL;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    grpc_metric *metric = registry->metrics;
    while (metric) {
        if (strcmp(metric->name, name) == 0) {
            pthread_mutex_unlock(&registry->mutex);
            return metric;
        }
        metric = metric->next;
    }
    
    pthread_mutex_unlock(&registry->mutex);
    return NULL;
}

void grpc_metrics_registry_destroy(grpc_metrics_registry *registry) {
    if (!registry) return;
    
    pthread_mutex_lock(&registry->mutex);
    
    grpc_metric *metric = registry->metrics;
    while (metric) {
        grpc_metric *next = metric->next;
        free(metric->name);
        free(metric->description);
        free(metric);
        metric = next;
    }
    
    pthread_mutex_unlock(&registry->mutex);
    pthread_mutex_destroy(&registry->mutex);
    
    free(registry);
}

/* ========================================================================
 * Logger API
 * ======================================================================== */

grpc_logger *grpc_logger_create(grpc_log_level min_level) {
    grpc_logger *logger = (grpc_logger *)calloc(1, sizeof(grpc_logger));
    if (!logger) {
        return NULL;
    }
    
    logger->min_level = min_level;
    logger->log_func = NULL;
    logger->user_data = NULL;
    pthread_mutex_init(&logger->mutex, NULL);
    
    return logger;
}

void grpc_logger_set_handler(grpc_logger *logger,
                             void (*log_func)(grpc_log_level, const char *, void *),
                             void *user_data) {
    if (!logger) return;
    
    pthread_mutex_lock(&logger->mutex);
    logger->log_func = log_func;
    logger->user_data = user_data;
    pthread_mutex_unlock(&logger->mutex);
}

void grpc_logger_log(grpc_logger *logger, grpc_log_level level, const char *message) {
    if (!logger || !message || level < logger->min_level) {
        return;
    }
    
    pthread_mutex_lock(&logger->mutex);
    
    if (logger->log_func) {
        logger->log_func(level, message, logger->user_data);
    }
    
    pthread_mutex_unlock(&logger->mutex);
}

void grpc_logger_destroy(grpc_logger *logger) {
    if (!logger) return;
    
    pthread_mutex_destroy(&logger->mutex);
    free(logger);
}
