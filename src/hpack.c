/**
 * @file hpack.c
 * @brief HPACK header compression implementation (RFC 7541)
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* HPACK static table size */
#define HPACK_STATIC_TABLE_SIZE 62

/* Static table entry */
typedef struct {
    const char *name;
    const char *value;
} hpack_static_entry;

/* HPACK static table (partial - most common headers) */
static const hpack_static_entry hpack_static_table[HPACK_STATIC_TABLE_SIZE] = {
    {NULL, NULL},  /* Index 0 is not used */
    {":authority", ""},
    {":method", "GET"},
    {":method", "POST"},
    {":path", "/"},
    {":path", "/index.html"},
    {":scheme", "http"},
    {":scheme", "https"},
    {":status", "200"},
    {":status", "204"},
    {":status", "206"},
    {":status", "304"},
    {":status", "400"},
    {":status", "404"},
    {":status", "500"},
    {"accept-charset", ""},
    {"accept-encoding", "gzip, deflate"},
    {"accept-language", ""},
    {"accept-ranges", ""},
    {"accept", ""},
    {"access-control-allow-origin", ""},
    {"age", ""},
    {"allow", ""},
    {"authorization", ""},
    {"cache-control", ""},
    {"content-disposition", ""},
    {"content-encoding", ""},
    {"content-language", ""},
    {"content-length", ""},
    {"content-location", ""},
    {"content-range", ""},
    {"content-type", ""},
    {"cookie", ""},
    {"date", ""},
    {"etag", ""},
    {"expect", ""},
    {"expires", ""},
    {"from", ""},
    {"host", ""},
    {"if-match", ""},
    {"if-modified-since", ""},
    {"if-none-match", ""},
    {"if-range", ""},
    {"if-unmodified-since", ""},
    {"last-modified", ""},
    {"link", ""},
    {"location", ""},
    {"max-forwards", ""},
    {"proxy-authenticate", ""},
    {"proxy-authorization", ""},
    {"range", ""},
    {"referer", ""},
    {"refresh", ""},
    {"retry-after", ""},
    {"server", ""},
    {"set-cookie", ""},
    {"strict-transport-security", ""},
    {"transfer-encoding", ""},
    {"user-agent", ""},
    {"vary", ""},
    {"via", ""},
    {"www-authenticate", ""}
};

/**
 * Encode integer using HPACK integer encoding
 * @param value The integer value to encode
 * @param prefix_bits Number of bits available in the first byte
 * @param output Output buffer
 * @param output_len Length of output buffer
 * @return Number of bytes written, or -1 on error
 */
int hpack_encode_integer(uint32_t value, uint8_t prefix_bits, uint8_t *output, size_t output_len) {
    if (!output || output_len == 0 || prefix_bits > 7) {
        return -1;
    }
    
    uint32_t max_prefix = (1U << prefix_bits) - 1;
    size_t pos = 0;
    
    if (value < max_prefix) {
        if (pos >= output_len) return -1;
        output[pos++] = (uint8_t)value;
    } else {
        if (pos >= output_len) return -1;
        output[pos++] = (uint8_t)max_prefix;
        value -= max_prefix;
        
        while (value >= 128) {
            if (pos >= output_len) return -1;
            output[pos++] = (uint8_t)((value & 0x7F) | 0x80);
            value >>= 7;
        }
        
        if (pos >= output_len) return -1;
        output[pos++] = (uint8_t)value;
    }
    
    return pos;
}

/**
 * Decode integer using HPACK integer encoding
 * @param input Input buffer
 * @param input_len Length of input buffer
 * @param prefix_bits Number of bits available in the first byte
 * @param value Output value
 * @return Number of bytes read, or -1 on error
 */
int hpack_decode_integer(const uint8_t *input, size_t input_len, uint8_t prefix_bits, uint32_t *value) {
    if (!input || !value || input_len == 0 || prefix_bits > 7) {
        return -1;
    }
    
    uint32_t max_prefix = (1U << prefix_bits) - 1;
    *value = input[0] & max_prefix;
    
    if (*value < max_prefix) {
        return 1;
    }
    
    size_t pos = 1;
    uint32_t m = 0;
    
    while (pos < input_len) {
        uint8_t byte = input[pos++];
        *value += (byte & 0x7F) * (1U << m);
        m += 7;
        
        if ((byte & 0x80) == 0) {
            return pos;
        }
        
        if (m > 28) {  /* Prevent overflow */
            return -1;
        }
    }
    
    return -1;  /* Incomplete integer */
}

/**
 * Encode a header field using literal representation
 * @param name Header name
 * @param value Header value
 * @param output Output buffer
 * @param output_len Length of output buffer
 * @return Number of bytes written, or -1 on error
 */
int hpack_encode_literal_header(const char *name, const char *value, uint8_t *output, size_t output_len) {
    if (!name || !value || !output || output_len == 0) {
        return -1;
    }
    
    size_t name_len = strlen(name);
    size_t value_len = strlen(value);
    size_t pos = 0;
    
    /* Check if we have space for the header */
    size_t required = 2 + name_len + value_len;
    if (output_len < required) {
        return -1;
    }
    
    /* Use literal header field without indexing (0x00 prefix) */
    output[pos++] = 0x00;
    
    /* Encode name length (7-bit prefix) */
    int len_bytes = hpack_encode_integer(name_len, 7, &output[pos], output_len - pos);
    if (len_bytes < 0) return -1;
    pos += len_bytes;
    
    /* Encode name */
    if (pos + name_len > output_len) return -1;
    memcpy(&output[pos], name, name_len);
    pos += name_len;
    
    /* Encode value length (7-bit prefix) */
    len_bytes = hpack_encode_integer(value_len, 7, &output[pos], output_len - pos);
    if (len_bytes < 0) return -1;
    pos += len_bytes;
    
    /* Encode value */
    if (pos + value_len > output_len) return -1;
    memcpy(&output[pos], value, value_len);
    pos += value_len;
    
    return pos;
}

/**
 * Encode metadata array using HPACK
 * @param metadata Metadata array to encode
 * @param output Output buffer
 * @param output_len Length of output buffer
 * @return Number of bytes written, or -1 on error
 */
int hpack_encode_metadata(const grpc_metadata_array *metadata, uint8_t *output, size_t output_len) {
    if (!metadata || !output) {
        return -1;
    }
    
    size_t pos = 0;
    
    for (size_t i = 0; i < metadata->count; i++) {
        const grpc_metadata *md = &metadata->metadata[i];
        
        /* For simplicity, use literal encoding for all headers */
        /* In a full implementation, we would check the static/dynamic tables */
        int bytes = hpack_encode_literal_header(md->key, md->value, &output[pos], output_len - pos);
        if (bytes < 0) {
            return -1;
        }
        pos += bytes;
    }
    
    return pos;
}

/**
 * Decode a literal header field
 * @param input Input buffer
 * @param input_len Length of input buffer
 * @param key Output key (allocated)
 * @param value Output value (allocated)
 * @return Number of bytes read, or -1 on error
 */
int hpack_decode_literal_header(const uint8_t *input, size_t input_len, char **key, char **value) {
    if (!input || !key || !value || input_len < 2) {
        return -1;
    }
    
    size_t pos = 1;  /* Skip the first byte (representation type) */
    
    /* Decode key length */
    uint32_t key_len;
    int len_bytes = hpack_decode_integer(&input[pos], input_len - pos, 7, &key_len);
    if (len_bytes < 0) return -1;
    pos += len_bytes;
    
    /* Read key */
    if (pos + key_len > input_len) return -1;
    *key = (char *)malloc(key_len + 1);
    if (!*key) return -1;
    memcpy(*key, &input[pos], key_len);
    (*key)[key_len] = '\0';
    pos += key_len;
    
    /* Decode value length */
    uint32_t value_len;
    len_bytes = hpack_decode_integer(&input[pos], input_len - pos, 7, &value_len);
    if (len_bytes < 0) {
        free(*key);
        return -1;
    }
    pos += len_bytes;
    
    /* Read value */
    if (pos + value_len > input_len) {
        free(*key);
        return -1;
    }
    *value = (char *)malloc(value_len + 1);
    if (!*value) {
        free(*key);
        return -1;
    }
    memcpy(*value, &input[pos], value_len);
    (*value)[value_len] = '\0';
    pos += value_len;
    
    return pos;
}

/**
 * Decode HPACK-encoded metadata
 * @param input Input buffer
 * @param input_len Length of input buffer
 * @param metadata Output metadata array
 * @return 0 on success, -1 on error
 */
int hpack_decode_metadata(const uint8_t *input, size_t input_len, grpc_metadata_array *metadata) {
    if (!input || !metadata) {
        return -1;
    }
    
    size_t pos = 0;
    
    /* Initialize metadata array */
    metadata->count = 0;
    metadata->capacity = 16;
    metadata->metadata = (grpc_metadata *)calloc(metadata->capacity, sizeof(grpc_metadata));
    if (!metadata->metadata) {
        return -1;
    }
    
    while (pos < input_len) {
        char *key = NULL;
        char *value = NULL;
        
        /* For simplicity, assume all headers are literals */
        /* In a full implementation, we would handle indexed headers */
        int bytes = hpack_decode_literal_header(&input[pos], input_len - pos, &key, &value);
        if (bytes < 0) {
            /* Cleanup on error */
            for (size_t i = 0; i < metadata->count; i++) {
                free((void *)metadata->metadata[i].key);
                free((void *)metadata->metadata[i].value);
            }
            free(metadata->metadata);
            return -1;
        }
        
        /* Add to metadata array */
        if (metadata->count >= metadata->capacity) {
            size_t new_capacity = metadata->capacity * 2;
            grpc_metadata *new_metadata = (grpc_metadata *)realloc(metadata->metadata,
                                                                     new_capacity * sizeof(grpc_metadata));
            if (!new_metadata) {
                free(key);
                free(value);
                return -1;
            }
            metadata->metadata = new_metadata;
            metadata->capacity = new_capacity;
        }
        
        metadata->metadata[metadata->count].key = key;
        metadata->metadata[metadata->count].value = value;
        metadata->metadata[metadata->count].value_length = strlen(value);
        metadata->count++;
        
        pos += bytes;
    }
    
    return 0;
}
