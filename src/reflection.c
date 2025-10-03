/**
 * @file reflection.c
 * @brief gRPC reflection API for service discovery
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Reflection Types
 * ======================================================================== */

/* Service descriptor */
typedef struct grpc_service_descriptor {
    char *service_name;
    char *package_name;
    struct grpc_method_descriptor *methods;
    size_t method_count;
    struct grpc_service_descriptor *next;
} grpc_service_descriptor;

/* Method descriptor */
typedef struct grpc_method_descriptor {
    char *method_name;
    char *input_type;
    char *output_type;
    bool client_streaming;
    bool server_streaming;
    struct grpc_method_descriptor *next;
} grpc_method_descriptor;

/* Reflection service registry */
typedef struct grpc_reflection_registry {
    grpc_service_descriptor *services;
    size_t service_count;
    pthread_mutex_t mutex;
} grpc_reflection_registry;

/* ========================================================================
 * Method Descriptor Management
 * ======================================================================== */

static grpc_method_descriptor *grpc_method_descriptor_create(const char *method_name,
                                                             const char *input_type,
                                                             const char *output_type,
                                                             bool client_streaming,
                                                             bool server_streaming) {
    grpc_method_descriptor *method = (grpc_method_descriptor *)calloc(1, sizeof(grpc_method_descriptor));
    if (!method) {
        return NULL;
    }
    
    method->method_name = strdup(method_name);
    if (!method->method_name) {
        free(method);
        return NULL;
    }
    
    method->input_type = strdup(input_type);
    if (!method->input_type) {
        free(method->method_name);
        free(method);
        return NULL;
    }
    
    method->output_type = strdup(output_type);
    if (!method->output_type) {
        free(method->input_type);
        free(method->method_name);
        free(method);
        return NULL;
    }
    
    method->client_streaming = client_streaming;
    method->server_streaming = server_streaming;
    method->next = NULL;
    
    return method;
}

static void grpc_method_descriptor_destroy(grpc_method_descriptor *method) {
    if (!method) return;
    
    free(method->method_name);
    free(method->input_type);
    free(method->output_type);
    free(method);
}

static void grpc_method_descriptor_list_destroy(grpc_method_descriptor *head) {
    while (head) {
        grpc_method_descriptor *next = head->next;
        grpc_method_descriptor_destroy(head);
        head = next;
    }
}

/* ========================================================================
 * Service Descriptor Management
 * ======================================================================== */

static grpc_service_descriptor *grpc_service_descriptor_create(const char *service_name,
                                                               const char *package_name) {
    grpc_service_descriptor *service = (grpc_service_descriptor *)calloc(1, sizeof(grpc_service_descriptor));
    if (!service) {
        return NULL;
    }
    
    service->service_name = strdup(service_name);
    if (!service->service_name) {
        free(service);
        return NULL;
    }
    
    if (package_name) {
        service->package_name = strdup(package_name);
        if (!service->package_name) {
            free(service->service_name);
            free(service);
            return NULL;
        }
    } else {
        service->package_name = NULL;
    }
    
    service->methods = NULL;
    service->method_count = 0;
    service->next = NULL;
    
    return service;
}

static void grpc_service_descriptor_destroy(grpc_service_descriptor *service) {
    if (!service) return;
    
    free(service->service_name);
    free(service->package_name);
    grpc_method_descriptor_list_destroy(service->methods);
    free(service);
}

static void grpc_service_descriptor_list_destroy(grpc_service_descriptor *head) {
    while (head) {
        grpc_service_descriptor *next = head->next;
        grpc_service_descriptor_destroy(head);
        head = next;
    }
}

/* ========================================================================
 * Reflection Registry API
 * ======================================================================== */

grpc_reflection_registry *grpc_reflection_registry_create(void) {
    grpc_reflection_registry *registry = (grpc_reflection_registry *)calloc(1, sizeof(grpc_reflection_registry));
    if (!registry) {
        return NULL;
    }
    
    registry->services = NULL;
    registry->service_count = 0;
    pthread_mutex_init(&registry->mutex, NULL);
    
    return registry;
}

int grpc_reflection_registry_add_service(grpc_reflection_registry *registry,
                                         const char *service_name,
                                         const char *package_name) {
    if (!registry || !service_name) {
        return -1;
    }
    
    grpc_service_descriptor *service = grpc_service_descriptor_create(service_name, package_name);
    if (!service) {
        return -1;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Add to list */
    service->next = registry->services;
    registry->services = service;
    registry->service_count++;
    
    pthread_mutex_unlock(&registry->mutex);
    return 0;
}

int grpc_reflection_registry_add_method(grpc_reflection_registry *registry,
                                       const char *service_name,
                                       const char *method_name,
                                       const char *input_type,
                                       const char *output_type,
                                       bool client_streaming,
                                       bool server_streaming) {
    if (!registry || !service_name || !method_name) {
        return -1;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Find service */
    grpc_service_descriptor *service = registry->services;
    while (service) {
        if (strcmp(service->service_name, service_name) == 0) {
            break;
        }
        service = service->next;
    }
    
    if (!service) {
        pthread_mutex_unlock(&registry->mutex);
        return -1;
    }
    
    /* Create method descriptor */
    grpc_method_descriptor *method = grpc_method_descriptor_create(method_name,
                                                                   input_type,
                                                                   output_type,
                                                                   client_streaming,
                                                                   server_streaming);
    if (!method) {
        pthread_mutex_unlock(&registry->mutex);
        return -1;
    }
    
    /* Add to service */
    method->next = service->methods;
    service->methods = method;
    service->method_count++;
    
    pthread_mutex_unlock(&registry->mutex);
    return 0;
}

grpc_service_descriptor *grpc_reflection_registry_list_services(grpc_reflection_registry *registry) {
    if (!registry) {
        return NULL;
    }
    
    pthread_mutex_lock(&registry->mutex);
    grpc_service_descriptor *services = registry->services;
    pthread_mutex_unlock(&registry->mutex);
    
    return services;
}

grpc_service_descriptor *grpc_reflection_registry_get_service(grpc_reflection_registry *registry,
                                                              const char *service_name) {
    if (!registry || !service_name) {
        return NULL;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    grpc_service_descriptor *service = registry->services;
    while (service) {
        if (strcmp(service->service_name, service_name) == 0) {
            pthread_mutex_unlock(&registry->mutex);
            return service;
        }
        service = service->next;
    }
    
    pthread_mutex_unlock(&registry->mutex);
    return NULL;
}

size_t grpc_reflection_registry_get_service_count(grpc_reflection_registry *registry) {
    if (!registry) {
        return 0;
    }
    
    pthread_mutex_lock(&registry->mutex);
    size_t count = registry->service_count;
    pthread_mutex_unlock(&registry->mutex);
    
    return count;
}

void grpc_reflection_registry_destroy(grpc_reflection_registry *registry) {
    if (!registry) return;
    
    pthread_mutex_lock(&registry->mutex);
    
    grpc_service_descriptor_list_destroy(registry->services);
    
    pthread_mutex_unlock(&registry->mutex);
    pthread_mutex_destroy(&registry->mutex);
    
    free(registry);
}

/* ========================================================================
 * Reflection Service Implementation
 * ======================================================================== */

/* Get full service name */
char *grpc_reflection_get_full_service_name(grpc_service_descriptor *service) {
    if (!service) {
        return NULL;
    }
    
    size_t len = strlen(service->service_name) + 1;
    if (service->package_name) {
        len += strlen(service->package_name) + 1; /* +1 for dot separator */
    }
    
    char *full_name = (char *)malloc(len);
    if (!full_name) {
        return NULL;
    }
    
    if (service->package_name) {
        snprintf(full_name, len, "%s.%s", service->package_name, service->service_name);
    } else {
        snprintf(full_name, len, "%s", service->service_name);
    }
    
    return full_name;
}

/* Get method full name */
char *grpc_reflection_get_full_method_name(grpc_service_descriptor *service,
                                          grpc_method_descriptor *method) {
    if (!service || !method) {
        return NULL;
    }
    
    char *service_name = grpc_reflection_get_full_service_name(service);
    if (!service_name) {
        return NULL;
    }
    
    size_t len = strlen(service_name) + strlen(method->method_name) + 2; /* +1 for slash, +1 for null */
    char *full_name = (char *)malloc(len);
    if (!full_name) {
        free(service_name);
        return NULL;
    }
    
    snprintf(full_name, len, "/%s/%s", service_name, method->method_name);
    free(service_name);
    
    return full_name;
}
