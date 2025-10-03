/**
 * @file name_resolver.c
 * @brief Name resolution and service discovery for gRPC
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>

/* ========================================================================
 * Name Resolver Types
 * ======================================================================== */

typedef enum {
    GRPC_RESOLVER_DNS,
    GRPC_RESOLVER_STATIC,
    GRPC_RESOLVER_CUSTOM
} grpc_resolver_type;

/* Resolved address */
typedef struct grpc_resolved_address {
    char *address;
    int port;
    struct grpc_resolved_address *next;
} grpc_resolved_address;

/* Name resolver */
typedef struct grpc_name_resolver {
    grpc_resolver_type type;
    char *target;
    grpc_resolved_address *addresses;
    size_t address_count;
    pthread_mutex_t mutex;
    /* Custom resolver callback */
    grpc_resolved_address *(*custom_resolve)(const char *target, void *user_data);
    void *user_data;
} grpc_name_resolver;

/* ========================================================================
 * Resolved Address Management
 * ======================================================================== */

static grpc_resolved_address *grpc_resolved_address_create(const char *address, int port) {
    grpc_resolved_address *addr = (grpc_resolved_address *)calloc(1, sizeof(grpc_resolved_address));
    if (!addr) {
        return NULL;
    }
    
    addr->address = strdup(address);
    if (!addr->address) {
        free(addr);
        return NULL;
    }
    
    addr->port = port;
    addr->next = NULL;
    
    return addr;
}

static void grpc_resolved_address_destroy(grpc_resolved_address *addr) {
    if (!addr) return;
    
    free(addr->address);
    free(addr);
}

static void grpc_resolved_address_list_destroy(grpc_resolved_address *head) {
    while (head) {
        grpc_resolved_address *next = head->next;
        grpc_resolved_address_destroy(head);
        head = next;
    }
}

/* ========================================================================
 * DNS Resolver
 * ======================================================================== */

static grpc_resolved_address *grpc_dns_resolve(const char *target) {
    if (!target) {
        return NULL;
    }
    
    /* Parse target: hostname:port or just hostname */
    char hostname[256];
    int port = 50051;  /* Default gRPC port */
    
    const char *colon = strchr(target, ':');
    if (colon) {
        size_t hostname_len = colon - target;
        if (hostname_len >= sizeof(hostname)) {
            return NULL;
        }
        strncpy(hostname, target, hostname_len);
        hostname[hostname_len] = '\0';
        port = atoi(colon + 1);
    } else {
        strncpy(hostname, target, sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }
    
    /* Resolve DNS */
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        return NULL;
    }
    
    /* Convert resolved addresses to our format */
    grpc_resolved_address *head = NULL;
    grpc_resolved_address *tail = NULL;
    
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        char addr_str[INET6_ADDRSTRLEN];
        void *addr_ptr;
        
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr_ptr = &ipv4->sin_addr;
        } else if (rp->ai_family == AF_INET6) {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
            addr_ptr = &ipv6->sin6_addr;
        } else {
            continue;
        }
        
        if (inet_ntop(rp->ai_family, addr_ptr, addr_str, sizeof(addr_str)) == NULL) {
            continue;
        }
        
        grpc_resolved_address *new_addr = grpc_resolved_address_create(addr_str, port);
        if (!new_addr) {
            continue;
        }
        
        if (!head) {
            head = new_addr;
            tail = new_addr;
        } else {
            tail->next = new_addr;
            tail = new_addr;
        }
    }
    
    freeaddrinfo(result);
    return head;
}

/* ========================================================================
 * Static Resolver
 * ======================================================================== */

static grpc_resolved_address *grpc_static_resolve(const char *target) {
    if (!target) {
        return NULL;
    }
    
    /* Parse target: address:port */
    char address[256];
    int port = 50051;
    
    const char *colon = strchr(target, ':');
    if (colon) {
        size_t addr_len = colon - target;
        if (addr_len >= sizeof(address)) {
            return NULL;
        }
        strncpy(address, target, addr_len);
        address[addr_len] = '\0';
        port = atoi(colon + 1);
    } else {
        strncpy(address, target, sizeof(address) - 1);
        address[sizeof(address) - 1] = '\0';
    }
    
    return grpc_resolved_address_create(address, port);
}

/* ========================================================================
 * Name Resolver API
 * ======================================================================== */

grpc_name_resolver *grpc_name_resolver_create(grpc_resolver_type type, const char *target) {
    if (!target) {
        return NULL;
    }
    
    grpc_name_resolver *resolver = (grpc_name_resolver *)calloc(1, sizeof(grpc_name_resolver));
    if (!resolver) {
        return NULL;
    }
    
    resolver->type = type;
    resolver->target = strdup(target);
    if (!resolver->target) {
        free(resolver);
        return NULL;
    }
    
    resolver->addresses = NULL;
    resolver->address_count = 0;
    resolver->custom_resolve = NULL;
    resolver->user_data = NULL;
    pthread_mutex_init(&resolver->mutex, NULL);
    
    return resolver;
}

int grpc_name_resolver_resolve(grpc_name_resolver *resolver) {
    if (!resolver) {
        return -1;
    }
    
    pthread_mutex_lock(&resolver->mutex);
    
    /* Clear existing addresses */
    grpc_resolved_address_list_destroy(resolver->addresses);
    resolver->addresses = NULL;
    resolver->address_count = 0;
    
    /* Resolve based on type */
    grpc_resolved_address *resolved = NULL;
    
    switch (resolver->type) {
        case GRPC_RESOLVER_DNS:
            resolved = grpc_dns_resolve(resolver->target);
            break;
        case GRPC_RESOLVER_STATIC:
            resolved = grpc_static_resolve(resolver->target);
            break;
        case GRPC_RESOLVER_CUSTOM:
            if (resolver->custom_resolve) {
                resolved = resolver->custom_resolve(resolver->target, resolver->user_data);
            }
            break;
        default:
            pthread_mutex_unlock(&resolver->mutex);
            return -1;
    }
    
    if (!resolved) {
        pthread_mutex_unlock(&resolver->mutex);
        return -1;
    }
    
    /* Count addresses */
    resolver->addresses = resolved;
    grpc_resolved_address *addr = resolved;
    while (addr) {
        resolver->address_count++;
        addr = addr->next;
    }
    
    pthread_mutex_unlock(&resolver->mutex);
    return 0;
}

grpc_resolved_address *grpc_name_resolver_get_addresses(grpc_name_resolver *resolver) {
    if (!resolver) {
        return NULL;
    }
    
    pthread_mutex_lock(&resolver->mutex);
    grpc_resolved_address *addresses = resolver->addresses;
    pthread_mutex_unlock(&resolver->mutex);
    
    return addresses;
}

size_t grpc_name_resolver_get_address_count(grpc_name_resolver *resolver) {
    if (!resolver) {
        return 0;
    }
    
    pthread_mutex_lock(&resolver->mutex);
    size_t count = resolver->address_count;
    pthread_mutex_unlock(&resolver->mutex);
    
    return count;
}

int grpc_name_resolver_set_custom_resolver(grpc_name_resolver *resolver,
                                           grpc_resolved_address *(*custom_resolve)(const char *, void *),
                                           void *user_data) {
    if (!resolver || !custom_resolve) {
        return -1;
    }
    
    pthread_mutex_lock(&resolver->mutex);
    resolver->custom_resolve = custom_resolve;
    resolver->user_data = user_data;
    pthread_mutex_unlock(&resolver->mutex);
    
    return 0;
}

void grpc_name_resolver_destroy(grpc_name_resolver *resolver) {
    if (!resolver) return;
    
    pthread_mutex_lock(&resolver->mutex);
    
    grpc_resolved_address_list_destroy(resolver->addresses);
    free(resolver->target);
    
    pthread_mutex_unlock(&resolver->mutex);
    pthread_mutex_destroy(&resolver->mutex);
    
    free(resolver);
}
