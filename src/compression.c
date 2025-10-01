/**
 * @file compression.c
 * @brief Data compression/decompression support (gzip, identity)
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Compression algorithm identifiers */
#define GRPC_COMPRESS_NONE "identity"
#define GRPC_COMPRESS_GZIP "gzip"
#define GRPC_COMPRESS_DEFLATE "deflate"

/**
 * Compress data using gzip
 * @param input Input data
 * @param input_len Length of input data
 * @param output Output compressed data (allocated)
 * @param output_len Length of output data
 * @return 0 on success, -1 on error
 */
static int compress_gzip(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len) {
    if (!input || !output || !output_len || input_len == 0) {
        return -1;
    }
    
    /* Estimate output buffer size (add 12 bytes for gzip header + deflate overhead) */
    *output_len = compressBound(input_len) + 12;
    *output = (uint8_t *)malloc(*output_len);
    if (!*output) {
        return -1;
    }
    
    /* Initialize zlib stream */
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)input;
    stream.avail_in = input_len;
    stream.next_out = *output;
    stream.avail_out = *output_len;
    
    /* Use deflateInit2 with gzip header (windowBits + 16) */
    int ret = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                           15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        free(*output);
        *output = NULL;
        return -1;
    }
    
    /* Compress data */
    ret = deflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&stream);
        free(*output);
        *output = NULL;
        return -1;
    }
    
    *output_len = stream.total_out;
    deflateEnd(&stream);
    
    /* Shrink buffer to actual size */
    uint8_t *resized = (uint8_t *)realloc(*output, *output_len);
    if (resized) {
        *output = resized;
    }
    
    return 0;
}

/**
 * Decompress gzip data
 * @param input Compressed input data
 * @param input_len Length of input data
 * @param output Output decompressed data (allocated)
 * @param output_len Length of output data
 * @return 0 on success, -1 on error
 */
static int decompress_gzip(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len) {
    if (!input || !output || !output_len || input_len == 0) {
        return -1;
    }
    
    /* Start with output buffer 4x the input size */
    *output_len = input_len * 4;
    *output = (uint8_t *)malloc(*output_len);
    if (!*output) {
        return -1;
    }
    
    /* Initialize zlib stream */
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)input;
    stream.avail_in = input_len;
    stream.next_out = *output;
    stream.avail_out = *output_len;
    
    /* Use inflateInit2 with gzip header (windowBits + 16) */
    int ret = inflateInit2(&stream, 15 + 16);
    if (ret != Z_OK) {
        free(*output);
        *output = NULL;
        return -1;
    }
    
    /* Decompress data, growing buffer if needed */
    while (1) {
        ret = inflate(&stream, Z_NO_FLUSH);
        
        if (ret == Z_STREAM_END) {
            break;
        }
        
        if (ret == Z_BUF_ERROR && stream.avail_out == 0) {
            /* Need more output space */
            size_t new_size = *output_len * 2;
            uint8_t *new_output = (uint8_t *)realloc(*output, new_size);
            if (!new_output) {
                inflateEnd(&stream);
                free(*output);
                *output = NULL;
                return -1;
            }
            
            stream.next_out = new_output + *output_len;
            stream.avail_out = new_size - *output_len;
            *output = new_output;
            *output_len = new_size;
        } else if (ret != Z_OK) {
            inflateEnd(&stream);
            free(*output);
            *output = NULL;
            return -1;
        }
    }
    
    *output_len = stream.total_out;
    inflateEnd(&stream);
    
    /* Shrink buffer to actual size */
    uint8_t *resized = (uint8_t *)realloc(*output, *output_len);
    if (resized) {
        *output = resized;
    }
    
    return 0;
}

/**
 * Compress data using specified algorithm
 * @param input Input data
 * @param input_len Length of input data
 * @param output Output compressed data (allocated)
 * @param output_len Length of output data
 * @param algorithm Compression algorithm ("gzip", "deflate", or "identity")
 * @return 0 on success, -1 on error
 */
int grpc_compress_data(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len, const char *algorithm) {
    if (!input || !output || !output_len || !algorithm) {
        return -1;
    }
    
    if (strcmp(algorithm, GRPC_COMPRESS_NONE) == 0) {
        /* No compression - just copy */
        *output = (uint8_t *)malloc(input_len);
        if (!*output) {
            return -1;
        }
        memcpy(*output, input, input_len);
        *output_len = input_len;
        return 0;
    } else if (strcmp(algorithm, GRPC_COMPRESS_GZIP) == 0) {
        return compress_gzip(input, input_len, output, output_len);
    } else if (strcmp(algorithm, GRPC_COMPRESS_DEFLATE) == 0) {
        /* Use gzip but client should handle differently */
        return compress_gzip(input, input_len, output, output_len);
    }
    
    /* Unknown algorithm */
    return -1;
}

/**
 * Decompress data using specified algorithm
 * @param input Compressed input data
 * @param input_len Length of input data
 * @param output Output decompressed data (allocated)
 * @param output_len Length of output data
 * @param algorithm Compression algorithm ("gzip", "deflate", or "identity")
 * @return 0 on success, -1 on error
 */
int grpc_decompress_data(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len, const char *algorithm) {
    if (!input || !output || !output_len || !algorithm) {
        return -1;
    }
    
    if (strcmp(algorithm, GRPC_COMPRESS_NONE) == 0) {
        /* No decompression - just copy */
        *output = (uint8_t *)malloc(input_len);
        if (!*output) {
            return -1;
        }
        memcpy(*output, input, input_len);
        *output_len = input_len;
        return 0;
    } else if (strcmp(algorithm, GRPC_COMPRESS_GZIP) == 0) {
        return decompress_gzip(input, input_len, output, output_len);
    } else if (strcmp(algorithm, GRPC_COMPRESS_DEFLATE) == 0) {
        return decompress_gzip(input, input_len, output, output_len);
    }
    
    /* Unknown algorithm */
    return -1;
}
