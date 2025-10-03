/**
 * @file grpc_protobuf.h
 * @brief Protocol Buffers integration helper functions
 * 
 * This header provides convenience functions for working with Protocol Buffers
 * messages in gRPC-C. Include this header when using protobuf-c with grpc-c.
 */

#ifndef GRPC_PROTOBUF_H
#define GRPC_PROTOBUF_H

#include "grpc.h"
#include <protobuf-c/protobuf-c.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serialize a protobuf message to a byte buffer
 * @param msg The protobuf message
 * @return Byte buffer containing serialized message, or NULL on error
 */
grpc_byte_buffer *grpc_protobuf_serialize(const ProtobufCMessage *msg);

/**
 * @brief Deserialize a byte buffer into a protobuf message
 * @param buffer The byte buffer
 * @param descriptor The message descriptor
 * @param msg Output parameter for deserialized message (allocated by function)
 * @return 0 on success, -1 on error
 */
int grpc_protobuf_deserialize(const grpc_byte_buffer *buffer,
                               const ProtobufCMessageDescriptor *descriptor,
                               ProtobufCMessage **msg);

/**
 * @brief Free a protobuf message
 * @param msg The message to free
 */
void grpc_protobuf_free(ProtobufCMessage *msg);

/**
 * @brief Create a byte buffer from raw protobuf data
 * @param data The protobuf data
 * @param length The length of the data
 * @return A byte buffer containing the data, or NULL on error
 */
grpc_byte_buffer *grpc_protobuf_buffer_create(const uint8_t *data, size_t length);

/**
 * @brief Get the size of a protobuf message when serialized
 * @param msg The protobuf message
 * @return The size of the serialized message, or 0 on error
 */
size_t grpc_protobuf_message_size(const ProtobufCMessage *msg);

/**
 * @brief Serialize a protobuf message directly to a pre-allocated buffer
 * @param msg The protobuf message to serialize
 * @param buffer The buffer to serialize into
 * @param buffer_size The size of the buffer
 * @return The number of bytes written, or 0 on error
 */
size_t grpc_protobuf_serialize_to_buffer(const ProtobufCMessage *msg,
                                         uint8_t *buffer,
                                         size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* GRPC_PROTOBUF_H */
