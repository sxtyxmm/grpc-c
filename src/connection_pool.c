/**
 * @file connection_pool.c
 * @brief Connection pooling and keep-alive for gRPC
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ========================================================================
 * Connection Pool Types
 * ======================================================================== */

/* Keep-alive configuration */
typedef struct {
    int interval_ms;        /* Interval between keep-alive pings */
    int timeout_ms;         /* Timeout for keep-alive response */
    bool permit_without_calls; /* Send keep-alive even when no calls active */
} grpc_keepalive_config;

/* Pooled connection */
typedef struct grpc_pooled_connection {
    char *target;
    http2_connection *connection;
    time_t last_used;
    time_t last_keepalive;
    int active_calls;
    bool is_healthy;
    struct grpc_pooled_connection *next;
} grpc_pooled_connection;

/* Connection pool */
typedef struct grpc_connection_pool {
    grpc_pooled_connection *connections;
    size_t max_connections;
    size_t current_connections;
    int idle_timeout_ms;
    grpc_keepalive_config keepalive;
    pthread_mutex_t mutex;
    pthread_t keepalive_thread;
    bool keepalive_running;
} grpc_connection_pool;

/* ========================================================================
 * Pooled Connection Management
 * ======================================================================== */

static grpc_pooled_connection *grpc_pooled_connection_create(const char *target,
                                                              http2_connection *connection) {
    grpc_pooled_connection *conn = (grpc_pooled_connection *)calloc(1, sizeof(grpc_pooled_connection));
    if (!conn) {
        return NULL;
    }
    
    conn->target = strdup(target);
    if (!conn->target) {
        free(conn);
        return NULL;
    }
    
    conn->connection = connection;
    conn->last_used = time(NULL);
    conn->last_keepalive = time(NULL);
    conn->active_calls = 0;
    conn->is_healthy = true;
    conn->next = NULL;
    
    return conn;
}

static void grpc_pooled_connection_destroy(grpc_pooled_connection *conn) {
    if (!conn) return;
    
    if (conn->connection) {
        http2_connection_destroy(conn->connection);
    }
    
    free(conn->target);
    free(conn);
}

/* ========================================================================
 * Keep-Alive Thread
 * ======================================================================== */

static void *grpc_keepalive_thread_func(void *arg) {
    grpc_connection_pool *pool = (grpc_connection_pool *)arg;
    
    while (pool->keepalive_running) {
        pthread_mutex_lock(&pool->mutex);
        
        time_t now = time(NULL);
        grpc_pooled_connection *conn = pool->connections;
        
        while (conn) {
            /* Check if keep-alive is needed */
            time_t elapsed = now - conn->last_keepalive;
            bool should_keepalive = (elapsed * 1000 >= pool->keepalive.interval_ms);
            
            if (should_keepalive && conn->is_healthy) {
                if (pool->keepalive.permit_without_calls || conn->active_calls > 0) {
                    /* Send HTTP/2 PING frame for keep-alive */
                    /* This would call http2_send_ping_frame(conn->connection) */
                    conn->last_keepalive = now;
                }
            }
            
            /* Check for idle timeout */
            if (conn->active_calls == 0 && pool->idle_timeout_ms > 0) {
                time_t idle_time = now - conn->last_used;
                if (idle_time * 1000 >= pool->idle_timeout_ms) {
                    conn->is_healthy = false;
                }
            }
            
            conn = conn->next;
        }
        
        pthread_mutex_unlock(&pool->mutex);
        
        /* Sleep for a short interval */
        usleep(100000); /* 100ms */
    }
    
    return NULL;
}

/* ========================================================================
 * Connection Pool API
 * ======================================================================== */

grpc_connection_pool *grpc_connection_pool_create(size_t max_connections, int idle_timeout_ms) {
    grpc_connection_pool *pool = (grpc_connection_pool *)calloc(1, sizeof(grpc_connection_pool));
    if (!pool) {
        return NULL;
    }
    
    pool->connections = NULL;
    pool->max_connections = max_connections > 0 ? max_connections : 10;
    pool->current_connections = 0;
    pool->idle_timeout_ms = idle_timeout_ms > 0 ? idle_timeout_ms : 30000; /* 30s default */
    
    /* Default keep-alive configuration */
    pool->keepalive.interval_ms = 30000;  /* 30 seconds */
    pool->keepalive.timeout_ms = 10000;   /* 10 seconds */
    pool->keepalive.permit_without_calls = false;
    
    pthread_mutex_init(&pool->mutex, NULL);
    
    /* Start keep-alive thread */
    pool->keepalive_running = true;
    if (pthread_create(&pool->keepalive_thread, NULL, grpc_keepalive_thread_func, pool) != 0) {
        pool->keepalive_running = false;
    }
    
    return pool;
}

int grpc_connection_pool_set_keepalive(grpc_connection_pool *pool,
                                       int interval_ms,
                                       int timeout_ms,
                                       bool permit_without_calls) {
    if (!pool) {
        return -1;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    pool->keepalive.interval_ms = interval_ms > 0 ? interval_ms : 30000;
    pool->keepalive.timeout_ms = timeout_ms > 0 ? timeout_ms : 10000;
    pool->keepalive.permit_without_calls = permit_without_calls;
    
    pthread_mutex_unlock(&pool->mutex);
    
    return 0;
}

http2_connection *grpc_connection_pool_get(grpc_connection_pool *pool, const char *target) {
    if (!pool || !target) {
        return NULL;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Look for existing healthy connection */
    grpc_pooled_connection *conn = pool->connections;
    while (conn) {
        if (conn->is_healthy && strcmp(conn->target, target) == 0) {
            conn->last_used = time(NULL);
            conn->active_calls++;
            pthread_mutex_unlock(&pool->mutex);
            return conn->connection;
        }
        conn = conn->next;
    }
    
    /* Check if we can create a new connection */
    if (pool->current_connections >= pool->max_connections) {
        /* Pool is full, try to reuse oldest idle connection */
        grpc_pooled_connection *oldest = NULL;
        time_t oldest_time = 0;
        
        conn = pool->connections;
        while (conn) {
            if (conn->active_calls == 0) {
                time_t idle = time(NULL) - conn->last_used;
                if (!oldest || idle > oldest_time) {
                    oldest = conn;
                    oldest_time = idle;
                }
            }
            conn = conn->next;
        }
        
        if (oldest) {
            /* Reuse this connection slot */
            grpc_pooled_connection_destroy(oldest);
            pool->current_connections--;
        } else {
            /* No idle connections available */
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }
    }
    
    /* Create new connection */
    http2_connection *new_conn = http2_connection_create(target, true, NULL);
    if (!new_conn) {
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }
    
    grpc_pooled_connection *pooled = grpc_pooled_connection_create(target, new_conn);
    if (!pooled) {
        http2_connection_destroy(new_conn);
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }
    
    /* Add to pool */
    pooled->next = pool->connections;
    pool->connections = pooled;
    pool->current_connections++;
    pooled->active_calls++;
    
    pthread_mutex_unlock(&pool->mutex);
    return new_conn;
}

int grpc_connection_pool_return(grpc_connection_pool *pool, const char *target, http2_connection *connection) {
    if (!pool || !target || !connection) {
        return -1;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Find connection in pool */
    grpc_pooled_connection *conn = pool->connections;
    while (conn) {
        if (conn->connection == connection && strcmp(conn->target, target) == 0) {
            if (conn->active_calls > 0) {
                conn->active_calls--;
            }
            conn->last_used = time(NULL);
            pthread_mutex_unlock(&pool->mutex);
            return 0;
        }
        conn = conn->next;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return -1;
}

void grpc_connection_pool_cleanup_idle(grpc_connection_pool *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    grpc_pooled_connection *prev = NULL;
    grpc_pooled_connection *conn = pool->connections;
    
    while (conn) {
        grpc_pooled_connection *next = conn->next;
        
        /* Check if connection is idle and timed out */
        if (conn->active_calls == 0 && !conn->is_healthy) {
            /* Remove from list */
            if (prev) {
                prev->next = next;
            } else {
                pool->connections = next;
            }
            
            grpc_pooled_connection_destroy(conn);
            pool->current_connections--;
            
            conn = next;
        } else {
            prev = conn;
            conn = next;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

void grpc_connection_pool_destroy(grpc_connection_pool *pool) {
    if (!pool) return;
    
    /* Stop keep-alive thread */
    pool->keepalive_running = false;
    pthread_join(pool->keepalive_thread, NULL);
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Destroy all connections */
    grpc_pooled_connection *conn = pool->connections;
    while (conn) {
        grpc_pooled_connection *next = conn->next;
        grpc_pooled_connection_destroy(conn);
        conn = next;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    
    free(pool);
}
