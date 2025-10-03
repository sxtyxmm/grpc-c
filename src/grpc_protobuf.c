/**
 * @file grpc_protobuf.c
 * @brief Protocol Buffers serialization/deserialization support using protobuf-c
 */

#define _POSIX_C_SOURCE 200809L
#include "grpc/grpc.h"
#include "grpc_internal.h"
#include <protobuf-c/protobuf-c.h>
#include <stdlib.h>
#include <string.h>

/**
 * Serialize a protobuf message to a byte buffer
 * @param msg The protobuf message to serialize
 * @return A byte buffer containing the serialized message, or NULL on error
 */
grpc_byte_buffer *grpc_protobuf_serialize(const ProtobufCMessage *msg) {
    if (!msg) {
        return NULL;
    }
    
    /* Calculate the size of the serialized message */
    size_t packed_size = protobuf_c_message_get_packed_size(msg);
    if (packed_size == 0) {
        return NULL;
    }
    
    /* Allocate buffer for serialized data */
    uint8_t *data = (uint8_t *)malloc(packed_size);
    if (!data) {
        return NULL;
    }
    
    /* Serialize the message */
    size_t packed = protobuf_c_message_pack(msg, data);
    if (packed != packed_size) {
        free(data);
        return NULL;
    }
    
    /* Create byte buffer */
    grpc_byte_buffer *buffer = grpc_byte_buffer_create(data, packed_size);
    free(data); /* grpc_byte_buffer_create copies the data */
    
    return buffer;
}

/**
 * Deserialize a byte buffer into a protobuf message
 * @param buffer The byte buffer containing the serialized message
 * @param descriptor The message descriptor for the protobuf type
 * @param msg Output parameter for the deserialized message (allocated by function)
 * @return 0 on success, -1 on error
 */
int grpc_protobuf_deserialize(const grpc_byte_buffer *buffer,
                               const ProtobufCMessageDescriptor *descriptor,
                               ProtobufCMessage **msg) {
    if (!buffer || !descriptor || !msg || !buffer->data) {
        return -1;
    }
    
    /* Unpack the message */
    *msg = protobuf_c_message_unpack(descriptor, NULL, buffer->length, buffer->data);
    if (!*msg) {
        return -1;
    }
    
    return 0;
}

/**
 * Free a protobuf message
 * @param msg The message to free
 */
void grpc_protobuf_free(ProtobufCMessage *msg) {
    if (msg) {
        protobuf_c_message_free_unpacked(msg, NULL);
    }
}

/**
 * Create a byte buffer from raw protobuf data
 * @param data The protobuf data
 * @param length The length of the data
 * @return A byte buffer containing the data, or NULL on error
 */
grpc_byte_buffer *grpc_protobuf_buffer_create(const uint8_t *data, size_t length) {
    return grpc_byte_buffer_create(data, length);
}

/**
 * Get the size of a protobuf message when serialized
 * @param msg The protobuf message
 * @return The size of the serialized message, or 0 on error
 */
size_t grpc_protobuf_message_size(const ProtobufCMessage *msg) {
    if (!msg) {
        return 0;
    }
    
    return protobuf_c_message_get_packed_size(msg);
}

/**
 * Serialize a protobuf message directly to a pre-allocated buffer
 * @param msg The protobuf message to serialize
 * @param buffer The buffer to serialize into
 * @param buffer_size The size of the buffer
 * @return The number of bytes written, or 0 on error
 */
size_t grpc_protobuf_serialize_to_buffer(const ProtobufCMessage *msg,
                                         uint8_t *buffer,
                                         size_t buffer_size) {
    if (!msg || !buffer) {
        return 0;
    }
    
    size_t packed_size = protobuf_c_message_get_packed_size(msg);
    if (packed_size > buffer_size) {
        return 0; /* Buffer too small */
    }
    
    return protobuf_c_message_pack(msg, buffer);
}
