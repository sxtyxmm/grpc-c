/**
 * @file grpc_core.c
 * @brief Core gRPC library implementation
 */

#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* Global initialization state */
static bool g_grpc_initialized = false;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ========================================================================
 * Library Initialization
 * ======================================================================== */

void grpc_init(void) {
    pthread_mutex_lock(&g_init_mutex);
    if (!g_grpc_initialized) {
        /* Initialize OpenSSL if needed */
        /* SSL_library_init(); */
        g_grpc_initialized = true;
    }
    pthread_mutex_unlock(&g_init_mutex);
}

void grpc_shutdown(void) {
    pthread_mutex_lock(&g_init_mutex);
    if (g_grpc_initialized) {
        /* Cleanup OpenSSL if needed */
        g_grpc_initialized = false;
    }
    pthread_mutex_unlock(&g_init_mutex);
}

/* ========================================================================
 * Completion Queue Implementation
 * ======================================================================== */

grpc_completion_queue *grpc_completion_queue_create(grpc_completion_type type) {
    grpc_completion_queue *cq = (grpc_completion_queue *)calloc(1, sizeof(grpc_completion_queue));
    if (!cq) {
        return NULL;
    }
    
    cq->type = type;
    pthread_mutex_init(&cq->mutex, NULL);
    pthread_cond_init(&cq->cond, NULL);
    cq->head = NULL;
    cq->tail = NULL;
    cq->shutdown = false;
    
    return cq;
}

void completion_queue_push_event(grpc_completion_queue *cq, grpc_event event) {
    if (!cq) return;
    
    completion_queue_event *ev = (completion_queue_event *)malloc(sizeof(completion_queue_event));
    if (!ev) return;
    
    ev->event = event;
    ev->next = NULL;
    
    pthread_mutex_lock(&cq->mutex);
    if (cq->tail) {
        cq->tail->next = ev;
    } else {
        cq->head = ev;
    }
    cq->tail = ev;
    pthread_cond_signal(&cq->cond);
    pthread_mutex_unlock(&cq->mutex);
}

grpc_event grpc_completion_queue_next(grpc_completion_queue *cq, grpc_timespec deadline) {
    grpc_event event = {0};
    
    if (!cq) {
        event.type = -1;
        return event;
    }
    
    pthread_mutex_lock(&cq->mutex);
    
    struct timespec ts;
    ts.tv_sec = deadline.tv_sec;
    ts.tv_nsec = deadline.tv_nsec;
    
    while (!cq->head && !cq->shutdown) {
        int ret = pthread_cond_timedwait(&cq->cond, &cq->mutex, &ts);
        if (ret != 0) {
            /* Timeout */
            event.type = 0;
            event.success = false;
            pthread_mutex_unlock(&cq->mutex);
            return event;
        }
    }
    
    if (cq->shutdown && !cq->head) {
        event.type = 1; /* GRPC_QUEUE_SHUTDOWN */
        event.success = false;
        pthread_mutex_unlock(&cq->mutex);
        return event;
    }
    
    completion_queue_event *ev = cq->head;
    cq->head = ev->next;
    if (!cq->head) {
        cq->tail = NULL;
    }
    
    event = ev->event;
    free(ev);
    
    pthread_mutex_unlock(&cq->mutex);
    return event;
}

void grpc_completion_queue_shutdown(grpc_completion_queue *cq) {
    if (!cq) return;
    
    pthread_mutex_lock(&cq->mutex);
    cq->shutdown = true;
    pthread_cond_broadcast(&cq->cond);
    pthread_mutex_unlock(&cq->mutex);
}

void grpc_completion_queue_destroy(grpc_completion_queue *cq) {
    if (!cq) return;
    
    pthread_mutex_lock(&cq->mutex);
    completion_queue_event *ev = cq->head;
    while (ev) {
        completion_queue_event *next = ev->next;
        free(ev);
        ev = next;
    }
    pthread_mutex_unlock(&cq->mutex);
    
    pthread_mutex_destroy(&cq->mutex);
    pthread_cond_destroy(&cq->cond);
    free(cq);
}

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

grpc_timespec grpc_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    grpc_timespec ts;
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    return ts;
}

grpc_timespec grpc_timeout_milliseconds_to_deadline(int64_t timeout_ms) {
    grpc_timespec now = grpc_now();
    now.tv_sec += timeout_ms / 1000;
    now.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    if (now.tv_nsec >= 1000000000) {
        now.tv_sec += now.tv_nsec / 1000000000;
        now.tv_nsec %= 1000000000;
    }
    
    return now;
}

grpc_byte_buffer *grpc_byte_buffer_create(const uint8_t *data, size_t length) {
    grpc_byte_buffer *buffer = (grpc_byte_buffer *)malloc(sizeof(grpc_byte_buffer));
    if (!buffer) {
        return NULL;
    }
    
    buffer->data = (uint8_t *)malloc(length);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    memcpy(buffer->data, data, length);
    buffer->length = length;
    buffer->capacity = length;
    
    return buffer;
}

void grpc_byte_buffer_destroy(grpc_byte_buffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    free(buffer);
}

const char *grpc_version_string(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             GRPC_C_VERSION_MAJOR,
             GRPC_C_VERSION_MINOR,
             GRPC_C_VERSION_PATCH);
    return version;
}
