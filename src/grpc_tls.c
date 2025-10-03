/**
 * @file grpc_tls.c
 * @brief TLS/SSL implementation using OpenSSL for secure connections
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc_internal.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Global SSL initialization flag */
static bool ssl_initialized = false;

/* Forward declaration */
static int grpc_ssl_alpn_select_cb_internal(SSL *ssl, const unsigned char **out,
                                            unsigned char *outlen,
                                            const unsigned char *in,
                                            unsigned int inlen, void *arg);

/**
 * Initialize OpenSSL library
 */
int grpc_ssl_init(void) {
    if (ssl_initialized) {
        return 0;
    }
    
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    ssl_initialized = true;
    
    return 0;
}

/**
 * Cleanup OpenSSL library
 */
void grpc_ssl_cleanup(void) {
    if (!ssl_initialized) {
        return;
    }
    
    EVP_cleanup();
    ERR_free_strings();
    ssl_initialized = false;
}

/**
 * Create SSL context for client
 */
void *grpc_ssl_create_client_context(const grpc_channel_credentials *creds) {
    if (!creds) {
        return NULL;
    }
    
    grpc_ssl_init();
    
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    /* Set minimum TLS version to 1.2 for security */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    
    /* Load root certificates for verification */
    if (creds->pem_root_certs) {
        BIO *bio = BIO_new_mem_buf(creds->pem_root_certs, -1);
        if (!bio) {
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return NULL;
        }
        
        X509_STORE *store = SSL_CTX_get_cert_store(ctx);
        X509 *cert;
        while ((cert = PEM_read_bio_X509(bio, NULL, 0, NULL)) != NULL) {
            X509_STORE_add_cert(store, cert);
            X509_free(cert);
        }
        BIO_free(bio);
    } else {
        /* Use system default CA certificates */
        if (!SSL_CTX_set_default_verify_paths(ctx)) {
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return NULL;
        }
    }
    
    /* Enable certificate verification */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    
    /* Load client certificate and key if provided */
    if (creds->pem_key_cert_pair) {
        grpc_ssl_pem_key_cert_pair *pair = (grpc_ssl_pem_key_cert_pair *)creds->pem_key_cert_pair;
        
        if (pair->cert_chain) {
            BIO *bio = BIO_new_mem_buf(pair->cert_chain, -1);
            if (bio) {
                X509 *cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
                if (cert) {
                    SSL_CTX_use_certificate(ctx, cert);
                    X509_free(cert);
                }
                BIO_free(bio);
            }
        }
        
        if (pair->private_key) {
            BIO *bio = BIO_new_mem_buf(pair->private_key, -1);
            if (bio) {
                EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL);
                if (pkey) {
                    SSL_CTX_use_PrivateKey(ctx, pkey);
                    EVP_PKEY_free(pkey);
                }
                BIO_free(bio);
            }
        }
    }
    
    /* Set ALPN for HTTP/2 */
    unsigned char alpn[] = "\x02h2";
    SSL_CTX_set_alpn_protos(ctx, alpn, sizeof(alpn) - 1);
    
    return ctx;
}

/**
 * Create SSL context for server
 */
void *grpc_ssl_create_server_context(const grpc_server_credentials *creds) {
    if (!creds || !creds->pem_key_cert_pairs || creds->num_key_cert_pairs == 0) {
        return NULL;
    }
    
    grpc_ssl_init();
    
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    /* Set minimum TLS version to 1.2 for security */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    
    /* Load server certificate and key */
    grpc_ssl_pem_key_cert_pair *pairs = (grpc_ssl_pem_key_cert_pair *)creds->pem_key_cert_pairs;
    grpc_ssl_pem_key_cert_pair *pair = &pairs[0]; /* Use first key/cert pair */
    
    if (pair->cert_chain) {
        BIO *bio = BIO_new_mem_buf(pair->cert_chain, -1);
        if (!bio) {
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return NULL;
        }
        
        X509 *cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
        if (!cert) {
            ERR_print_errors_fp(stderr);
            BIO_free(bio);
            SSL_CTX_free(ctx);
            return NULL;
        }
        
        if (SSL_CTX_use_certificate(ctx, cert) != 1) {
            ERR_print_errors_fp(stderr);
            X509_free(cert);
            BIO_free(bio);
            SSL_CTX_free(ctx);
            return NULL;
        }
        X509_free(cert);
        BIO_free(bio);
    } else {
        fprintf(stderr, "Server certificate chain is required\n");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    if (pair->private_key) {
        BIO *bio = BIO_new_mem_buf(pair->private_key, -1);
        if (!bio) {
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return NULL;
        }
        
        EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL);
        if (!pkey) {
            ERR_print_errors_fp(stderr);
            BIO_free(bio);
            SSL_CTX_free(ctx);
            return NULL;
        }
        
        if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1) {
            ERR_print_errors_fp(stderr);
            EVP_PKEY_free(pkey);
            BIO_free(bio);
            SSL_CTX_free(ctx);
            return NULL;
        }
        EVP_PKEY_free(pkey);
        BIO_free(bio);
    } else {
        fprintf(stderr, "Server private key is required\n");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    /* Verify that key and certificate match */
    if (SSL_CTX_check_private_key(ctx) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    /* Load client CA certificates if provided */
    if (creds->pem_root_certs) {
        BIO *bio = BIO_new_mem_buf(creds->pem_root_certs, -1);
        if (bio) {
            X509_STORE *store = SSL_CTX_get_cert_store(ctx);
            X509 *cert;
            while ((cert = PEM_read_bio_X509(bio, NULL, 0, NULL)) != NULL) {
                X509_STORE_add_cert(store, cert);
                X509_free(cert);
            }
            BIO_free(bio);
            
            /* Require client certificate if CA is provided */
            SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
        }
    }
    
    /* Set ALPN for HTTP/2 */
    SSL_CTX_set_alpn_select_cb(ctx, grpc_ssl_alpn_select_cb_internal, NULL);
    
    return ctx;
}

/**
 * ALPN callback for server (internal function)
 */
static int grpc_ssl_alpn_select_cb_internal(SSL *ssl, const unsigned char **out,
                                            unsigned char *outlen,
                                            const unsigned char *in,
                                            unsigned int inlen, void *arg) {
    (void)ssl;
    (void)arg;
    
    /* Look for "h2" protocol */
    for (unsigned int i = 0; i < inlen;) {
        unsigned char len = in[i];
        if (i + 1 + len > inlen) {
            break;
        }
        if (len == 2 && memcmp(&in[i + 1], "h2", 2) == 0) {
            *out = &in[i + 1];
            *outlen = len;
            return SSL_TLSEXT_ERR_OK;
        }
        i += 1 + len;
    }
    
    return SSL_TLSEXT_ERR_NOACK;
}

/**
 * Destroy SSL context
 */
void grpc_ssl_destroy_context(void *ssl_ctx) {
    if (ssl_ctx) {
        SSL_CTX_free((SSL_CTX *)ssl_ctx);
    }
}

/**
 * Perform SSL handshake for client connection
 */
int grpc_ssl_client_handshake(http2_connection *conn, const char *target_host) {
    if (!conn || !conn->ssl_ctx) {
        return -1;
    }
    
    SSL_CTX *ctx = (SSL_CTX *)conn->ssl_ctx;
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    /* Set hostname for SNI and certificate verification */
    if (target_host) {
        SSL_set_tlsext_host_name(ssl, target_host);
        /* Enable hostname verification */
        SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        SSL_set1_host(ssl, target_host);
    }
    
    SSL_set_fd(ssl, conn->socket_fd);
    
    /* Perform SSL handshake */
    int ret = SSL_connect(ssl);
    if (ret != 1) {
        int err = SSL_get_error(ssl, ret);
        fprintf(stderr, "SSL handshake failed: error code %d\n", err);
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return -1;
    }
    
    /* Verify certificate */
    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        fprintf(stderr, "Certificate verification failed\n");
        SSL_free(ssl);
        return -1;
    }
    
    /* Verify ALPN negotiated HTTP/2 */
    const unsigned char *alpn_proto;
    unsigned int alpn_len;
    SSL_get0_alpn_selected(ssl, &alpn_proto, &alpn_len);
    if (alpn_len != 2 || memcmp(alpn_proto, "h2", 2) != 0) {
        fprintf(stderr, "HTTP/2 not negotiated via ALPN\n");
        SSL_free(ssl);
        return -1;
    }
    
    conn->ssl = ssl;
    return 0;
}

/**
 * Perform SSL handshake for server connection
 */
int grpc_ssl_server_handshake(http2_connection *conn) {
    if (!conn || !conn->ssl_ctx) {
        return -1;
    }
    
    SSL_CTX *ctx = (SSL_CTX *)conn->ssl_ctx;
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    SSL_set_fd(ssl, conn->socket_fd);
    
    /* Perform SSL handshake */
    int ret = SSL_accept(ssl);
    if (ret != 1) {
        int err = SSL_get_error(ssl, ret);
        fprintf(stderr, "SSL handshake failed: error code %d\n", err);
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return -1;
    }
    
    conn->ssl = ssl;
    return 0;
}

/**
 * Read data from SSL connection
 */
ssize_t grpc_ssl_read(http2_connection *conn, void *buf, size_t len) {
    if (!conn || !conn->ssl) {
        return -1;
    }
    
    SSL *ssl = (SSL *)conn->ssl;
    int ret = SSL_read(ssl, buf, len);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0; /* Would block */
        }
        return -1;
    }
    
    return ret;
}

/**
 * Write data to SSL connection
 */
ssize_t grpc_ssl_write(http2_connection *conn, const void *buf, size_t len) {
    if (!conn || !conn->ssl) {
        return -1;
    }
    
    SSL *ssl = (SSL *)conn->ssl;
    int ret = SSL_write(ssl, buf, len);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0; /* Would block */
        }
        return -1;
    }
    
    return ret;
}

/**
 * Shutdown SSL connection
 */
void grpc_ssl_shutdown(http2_connection *conn) {
    if (conn && conn->ssl) {
        SSL *ssl = (SSL *)conn->ssl;
        SSL_shutdown(ssl);
        SSL_free(ssl);
        conn->ssl = NULL;
    }
}
