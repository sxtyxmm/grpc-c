/**
 * @file load_balancing.c
 * @brief Load balancing policies for gRPC channels
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================
 * Load Balancing Policy Types
 * ======================================================================== */

typedef enum {
    GRPC_LB_POLICY_ROUND_ROBIN,
    GRPC_LB_POLICY_PICK_FIRST,
    GRPC_LB_POLICY_WEIGHTED
} grpc_lb_policy_type;

/* Backend server address */
typedef struct grpc_lb_address {
    char *address;
    int weight;  /* For weighted load balancing */
    bool is_available;
    struct grpc_lb_address *next;
} grpc_lb_address;

/* Load balancing policy */
typedef struct grpc_lb_policy {
    grpc_lb_policy_type type;
    grpc_lb_address *addresses;
    size_t address_count;
    size_t current_index;  /* For round-robin */
    pthread_mutex_t mutex;
} grpc_lb_policy;

/* ========================================================================
 * Address List Management
 * ======================================================================== */

static grpc_lb_address *grpc_lb_address_create(const char *address, int weight) {
    grpc_lb_address *addr = (grpc_lb_address *)calloc(1, sizeof(grpc_lb_address));
    if (!addr) {
        return NULL;
    }
    
    addr->address = strdup(address);
    if (!addr->address) {
        free(addr);
        return NULL;
    }
    
    addr->weight = weight > 0 ? weight : 1;
    addr->is_available = true;
    addr->next = NULL;
    
    return addr;
}

static void grpc_lb_address_destroy(grpc_lb_address *addr) {
    if (!addr) return;
    
    free(addr->address);
    free(addr);
}

/* ========================================================================
 * Round-Robin Load Balancer
 * ======================================================================== */

static const char *grpc_lb_round_robin_pick(grpc_lb_policy *policy) {
    if (!policy || !policy->addresses || policy->address_count == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&policy->mutex);
    
    /* Find next available address */
    size_t attempts = 0;
    grpc_lb_address *addr = policy->addresses;
    size_t target_index = policy->current_index;
    
    /* Advance to target index */
    for (size_t i = 0; i < target_index && addr; i++) {
        addr = addr->next;
    }
    
    /* Find available address */
    while (addr && attempts < policy->address_count) {
        if (addr->is_available) {
            const char *result = addr->address;
            policy->current_index = (policy->current_index + 1) % policy->address_count;
            pthread_mutex_unlock(&policy->mutex);
            return result;
        }
        
        addr = addr->next;
        if (!addr) {
            addr = policy->addresses;
        }
        attempts++;
    }
    
    pthread_mutex_unlock(&policy->mutex);
    return NULL;
}

/* ========================================================================
 * Pick-First Load Balancer
 * ======================================================================== */

static const char *grpc_lb_pick_first_pick(grpc_lb_policy *policy) {
    if (!policy || !policy->addresses) {
        return NULL;
    }
    
    pthread_mutex_lock(&policy->mutex);
    
    /* Return first available address */
    grpc_lb_address *addr = policy->addresses;
    while (addr) {
        if (addr->is_available) {
            pthread_mutex_unlock(&policy->mutex);
            return addr->address;
        }
        addr = addr->next;
    }
    
    pthread_mutex_unlock(&policy->mutex);
    return NULL;
}

/* ========================================================================
 * Weighted Load Balancer
 * ======================================================================== */

static const char *grpc_lb_weighted_pick(grpc_lb_policy *policy) {
    if (!policy || !policy->addresses || policy->address_count == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&policy->mutex);
    
    /* Calculate total weight */
    int total_weight = 0;
    grpc_lb_address *addr = policy->addresses;
    while (addr) {
        if (addr->is_available) {
            total_weight += addr->weight;
        }
        addr = addr->next;
    }
    
    if (total_weight == 0) {
        pthread_mutex_unlock(&policy->mutex);
        return NULL;
    }
    
    /* Pick random weighted address */
    int random_weight = rand() % total_weight;
    int current_weight = 0;
    
    addr = policy->addresses;
    while (addr) {
        if (addr->is_available) {
            current_weight += addr->weight;
            if (current_weight > random_weight) {
                pthread_mutex_unlock(&policy->mutex);
                return addr->address;
            }
        }
        addr = addr->next;
    }
    
    pthread_mutex_unlock(&policy->mutex);
    return NULL;
}

/* ========================================================================
 * Load Balancing Policy API
 * ======================================================================== */

grpc_lb_policy *grpc_lb_policy_create(grpc_lb_policy_type type) {
    grpc_lb_policy *policy = (grpc_lb_policy *)calloc(1, sizeof(grpc_lb_policy));
    if (!policy) {
        return NULL;
    }
    
    policy->type = type;
    policy->addresses = NULL;
    policy->address_count = 0;
    policy->current_index = 0;
    pthread_mutex_init(&policy->mutex, NULL);
    
    /* Seed random number generator for weighted load balancing */
    srand((unsigned int)time(NULL));
    
    return policy;
}

int grpc_lb_policy_add_address(grpc_lb_policy *policy, const char *address, int weight) {
    if (!policy || !address) {
        return -1;
    }
    
    grpc_lb_address *new_addr = grpc_lb_address_create(address, weight);
    if (!new_addr) {
        return -1;
    }
    
    pthread_mutex_lock(&policy->mutex);
    
    /* Add to end of list */
    if (!policy->addresses) {
        policy->addresses = new_addr;
    } else {
        grpc_lb_address *last = policy->addresses;
        while (last->next) {
            last = last->next;
        }
        last->next = new_addr;
    }
    
    policy->address_count++;
    
    pthread_mutex_unlock(&policy->mutex);
    return 0;
}

const char *grpc_lb_policy_pick(grpc_lb_policy *policy) {
    if (!policy) {
        return NULL;
    }
    
    switch (policy->type) {
        case GRPC_LB_POLICY_ROUND_ROBIN:
            return grpc_lb_round_robin_pick(policy);
        case GRPC_LB_POLICY_PICK_FIRST:
            return grpc_lb_pick_first_pick(policy);
        case GRPC_LB_POLICY_WEIGHTED:
            return grpc_lb_weighted_pick(policy);
        default:
            return NULL;
    }
}

int grpc_lb_policy_mark_unavailable(grpc_lb_policy *policy, const char *address) {
    if (!policy || !address) {
        return -1;
    }
    
    pthread_mutex_lock(&policy->mutex);
    
    grpc_lb_address *addr = policy->addresses;
    while (addr) {
        if (strcmp(addr->address, address) == 0) {
            addr->is_available = false;
            pthread_mutex_unlock(&policy->mutex);
            return 0;
        }
        addr = addr->next;
    }
    
    pthread_mutex_unlock(&policy->mutex);
    return -1;
}

int grpc_lb_policy_mark_available(grpc_lb_policy *policy, const char *address) {
    if (!policy || !address) {
        return -1;
    }
    
    pthread_mutex_lock(&policy->mutex);
    
    grpc_lb_address *addr = policy->addresses;
    while (addr) {
        if (strcmp(addr->address, address) == 0) {
            addr->is_available = true;
            pthread_mutex_unlock(&policy->mutex);
            return 0;
        }
        addr = addr->next;
    }
    
    pthread_mutex_unlock(&policy->mutex);
    return -1;
}

void grpc_lb_policy_destroy(grpc_lb_policy *policy) {
    if (!policy) return;
    
    pthread_mutex_lock(&policy->mutex);
    
    grpc_lb_address *addr = policy->addresses;
    while (addr) {
        grpc_lb_address *next = addr->next;
        grpc_lb_address_destroy(addr);
        addr = next;
    }
    
    pthread_mutex_unlock(&policy->mutex);
    pthread_mutex_destroy(&policy->mutex);
    
    free(policy);
}
