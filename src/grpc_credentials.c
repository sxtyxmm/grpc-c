/**
 * @file grpc_credentials.c
 * @brief Credentials implementation for SSL/TLS support
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Channel Credentials
 * ======================================================================== */

grpc_channel_credentials *grpc_ssl_credentials_create(
    const char *pem_root_certs,
    void *pem_key_cert_pair) {
    
    grpc_channel_credentials *creds = (grpc_channel_credentials *)calloc(
        1, sizeof(grpc_channel_credentials));
    if (!creds) {
        return NULL;
    }
    
    if (pem_root_certs) {
        creds->pem_root_certs = strdup(pem_root_certs);
        if (!creds->pem_root_certs) {
            free(creds);
            return NULL;
        }
    }
    
    creds->pem_key_cert_pair = pem_key_cert_pair;
    
    return creds;
}

void grpc_channel_credentials_release(grpc_channel_credentials *creds) {
    if (!creds) return;
    
    free(creds->pem_root_certs);
    free(creds);
}

/* ========================================================================
 * Server Credentials
 * ======================================================================== */

grpc_server_credentials *grpc_ssl_server_credentials_create(
    const char *pem_root_certs,
    void *pem_key_cert_pairs,
    size_t num_key_cert_pairs) {
    
    grpc_server_credentials *creds = (grpc_server_credentials *)calloc(
        1, sizeof(grpc_server_credentials));
    if (!creds) {
        return NULL;
    }
    
    if (pem_root_certs) {
        creds->pem_root_certs = strdup(pem_root_certs);
        if (!creds->pem_root_certs) {
            free(creds);
            return NULL;
        }
    }
    
    creds->pem_key_cert_pairs = pem_key_cert_pairs;
    creds->num_key_cert_pairs = num_key_cert_pairs;
    
    return creds;
}

void grpc_server_credentials_release(grpc_server_credentials *creds) {
    if (!creds) return;
    
    free(creds->pem_root_certs);
    free(creds);
}
