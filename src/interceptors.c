/**
 * @file interceptors.c
 * @brief Client and server interceptors for gRPC
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Interceptor Types
 * ======================================================================== */

/* Interceptor context for client calls */
typedef struct grpc_client_interceptor_context {
    grpc_call *call;
    const char *method;
    const char *host;
    grpc_metadata_array *initial_metadata;
    grpc_byte_buffer *send_message;
    void *user_data;
} grpc_client_interceptor_context;

/* Interceptor context for server calls */
typedef struct grpc_server_interceptor_context {
    grpc_call *call;
    const char *method;
    grpc_metadata_array *initial_metadata;
    grpc_byte_buffer *recv_message;
    void *user_data;
} grpc_server_interceptor_context;

/* Client interceptor function signature */
typedef int (*grpc_client_interceptor_func)(grpc_client_interceptor_context *ctx);

/* Server interceptor function signature */
typedef int (*grpc_server_interceptor_func)(grpc_server_interceptor_context *ctx);

/* Interceptor chain node */
typedef struct grpc_interceptor_node {
    void *interceptor_func;
    void *user_data;
    struct grpc_interceptor_node *next;
} grpc_interceptor_node;

/* Client interceptor chain */
typedef struct grpc_client_interceptor_chain {
    grpc_interceptor_node *head;
    grpc_interceptor_node *tail;
    size_t count;
    pthread_mutex_t mutex;
} grpc_client_interceptor_chain;

/* Server interceptor chain */
typedef struct grpc_server_interceptor_chain {
    grpc_interceptor_node *head;
    grpc_interceptor_node *tail;
    size_t count;
    pthread_mutex_t mutex;
} grpc_server_interceptor_chain;

/* ========================================================================
 * Interceptor Node Management
 * ======================================================================== */

static grpc_interceptor_node *grpc_interceptor_node_create(void *func, void *user_data) {
    grpc_interceptor_node *node = (grpc_interceptor_node *)calloc(1, sizeof(grpc_interceptor_node));
    if (!node) {
        return NULL;
    }
    
    node->interceptor_func = func;
    node->user_data = user_data;
    node->next = NULL;
    
    return node;
}

static void grpc_interceptor_node_destroy(grpc_interceptor_node *node) {
    free(node);
}

/* ========================================================================
 * Client Interceptor Chain API
 * ======================================================================== */

grpc_client_interceptor_chain *grpc_client_interceptor_chain_create(void) {
    grpc_client_interceptor_chain *chain = (grpc_client_interceptor_chain *)calloc(1, sizeof(grpc_client_interceptor_chain));
    if (!chain) {
        return NULL;
    }
    
    chain->head = NULL;
    chain->tail = NULL;
    chain->count = 0;
    pthread_mutex_init(&chain->mutex, NULL);
    
    return chain;
}

int grpc_client_interceptor_chain_add(grpc_client_interceptor_chain *chain,
                                      grpc_client_interceptor_func interceptor,
                                      void *user_data) {
    if (!chain || !interceptor) {
        return -1;
    }
    
    grpc_interceptor_node *node = grpc_interceptor_node_create((void *)interceptor, user_data);
    if (!node) {
        return -1;
    }
    
    pthread_mutex_lock(&chain->mutex);
    
    if (!chain->head) {
        chain->head = node;
        chain->tail = node;
    } else {
        chain->tail->next = node;
        chain->tail = node;
    }
    
    chain->count++;
    
    pthread_mutex_unlock(&chain->mutex);
    return 0;
}

int grpc_client_interceptor_chain_execute(grpc_client_interceptor_chain *chain,
                                         grpc_call *call,
                                         const char *method,
                                         const char *host,
                                         grpc_metadata_array *initial_metadata,
                                         grpc_byte_buffer *send_message) {
    if (!chain || !call) {
        return 0;
    }
    
    pthread_mutex_lock(&chain->mutex);
    
    /* Create context */
    grpc_client_interceptor_context ctx;
    ctx.call = call;
    ctx.method = method;
    ctx.host = host;
    ctx.initial_metadata = initial_metadata;
    ctx.send_message = send_message;
    
    /* Execute interceptors in order */
    grpc_interceptor_node *node = chain->head;
    int result = 0;
    
    while (node && result == 0) {
        grpc_client_interceptor_func func = (grpc_client_interceptor_func)node->interceptor_func;
        ctx.user_data = node->user_data;
        result = func(&ctx);
        node = node->next;
    }
    
    pthread_mutex_unlock(&chain->mutex);
    return result;
}

void grpc_client_interceptor_chain_destroy(grpc_client_interceptor_chain *chain) {
    if (!chain) return;
    
    pthread_mutex_lock(&chain->mutex);
    
    grpc_interceptor_node *node = chain->head;
    while (node) {
        grpc_interceptor_node *next = node->next;
        grpc_interceptor_node_destroy(node);
        node = next;
    }
    
    pthread_mutex_unlock(&chain->mutex);
    pthread_mutex_destroy(&chain->mutex);
    
    free(chain);
}

/* ========================================================================
 * Server Interceptor Chain API
 * ======================================================================== */

grpc_server_interceptor_chain *grpc_server_interceptor_chain_create(void) {
    grpc_server_interceptor_chain *chain = (grpc_server_interceptor_chain *)calloc(1, sizeof(grpc_server_interceptor_chain));
    if (!chain) {
        return NULL;
    }
    
    chain->head = NULL;
    chain->tail = NULL;
    chain->count = 0;
    pthread_mutex_init(&chain->mutex, NULL);
    
    return chain;
}

int grpc_server_interceptor_chain_add(grpc_server_interceptor_chain *chain,
                                      grpc_server_interceptor_func interceptor,
                                      void *user_data) {
    if (!chain || !interceptor) {
        return -1;
    }
    
    grpc_interceptor_node *node = grpc_interceptor_node_create((void *)interceptor, user_data);
    if (!node) {
        return -1;
    }
    
    pthread_mutex_lock(&chain->mutex);
    
    if (!chain->head) {
        chain->head = node;
        chain->tail = node;
    } else {
        chain->tail->next = node;
        chain->tail = node;
    }
    
    chain->count++;
    
    pthread_mutex_unlock(&chain->mutex);
    return 0;
}

int grpc_server_interceptor_chain_execute(grpc_server_interceptor_chain *chain,
                                         grpc_call *call,
                                         const char *method,
                                         grpc_metadata_array *initial_metadata,
                                         grpc_byte_buffer *recv_message) {
    if (!chain || !call) {
        return 0;
    }
    
    pthread_mutex_lock(&chain->mutex);
    
    /* Create context */
    grpc_server_interceptor_context ctx;
    ctx.call = call;
    ctx.method = method;
    ctx.initial_metadata = initial_metadata;
    ctx.recv_message = recv_message;
    
    /* Execute interceptors in order */
    grpc_interceptor_node *node = chain->head;
    int result = 0;
    
    while (node && result == 0) {
        grpc_server_interceptor_func func = (grpc_server_interceptor_func)node->interceptor_func;
        ctx.user_data = node->user_data;
        result = func(&ctx);
        node = node->next;
    }
    
    pthread_mutex_unlock(&chain->mutex);
    return result;
}

void grpc_server_interceptor_chain_destroy(grpc_server_interceptor_chain *chain) {
    if (!chain) return;
    
    pthread_mutex_lock(&chain->mutex);
    
    grpc_interceptor_node *node = chain->head;
    while (node) {
        grpc_interceptor_node *next = node->next;
        grpc_interceptor_node_destroy(node);
        node = next;
    }
    
    pthread_mutex_unlock(&chain->mutex);
    pthread_mutex_destroy(&chain->mutex);
    
    free(chain);
}

/* ========================================================================
 * Example Interceptors
 * ======================================================================== */

/* Logging interceptor for client calls */
int grpc_logging_client_interceptor(grpc_client_interceptor_context *ctx) {
    if (!ctx) {
        return -1;
    }
    
    /* Log call information */
    /* In a real implementation, this would use a proper logging framework */
    /* For now, this is just a placeholder */
    
    return 0;
}

/* Logging interceptor for server calls */
int grpc_logging_server_interceptor(grpc_server_interceptor_context *ctx) {
    if (!ctx) {
        return -1;
    }
    
    /* Log call information */
    /* In a real implementation, this would use a proper logging framework */
    
    return 0;
}

/* Authentication interceptor for client calls */
int grpc_auth_client_interceptor(grpc_client_interceptor_context *ctx) {
    if (!ctx) {
        return -1;
    }
    
    /* Add authentication metadata */
    /* In a real implementation, this would add auth tokens/credentials */
    
    return 0;
}

/* Authentication interceptor for server calls */
int grpc_auth_server_interceptor(grpc_server_interceptor_context *ctx) {
    if (!ctx) {
        return -1;
    }
    
    /* Validate authentication metadata */
    /* In a real implementation, this would verify auth tokens/credentials */
    
    return 0;
}
