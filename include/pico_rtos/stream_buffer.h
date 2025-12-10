#ifndef PICO_RTOS_STREAM_BUFFER_H
#define PICO_RTOS_STREAM_BUFFER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "pico/critical_section.h"
#include "pico_rtos/types.h"
#include "pico_rtos/error.h"

/**
 * @file stream_buffer.h
 * @brief Stream Buffer Implementation for Pico-RTOS v0.3.1
 * 
 * Stream buffers provide efficient variable-length message passing between tasks
 * with optional zero-copy semantics for large data transfers. They use circular
 * buffer management for optimal memory utilization and support blocking operations
 * with configurable timeouts.
 * 
 * Key features:
 * - Variable-length message support
 * - Circular buffer with head/tail pointers
 * - Thread-safe atomic operations
 * - Blocking send/receive with timeout support
 * - Zero-copy optimization for large messages
 * - Integration with existing blocking system
 */

// Forward declare the blocking object structure
struct pico_rtos_block_object;

// =============================================================================
// CONSTANTS AND CONFIGURATION
// =============================================================================

/** @brief Minimum stream buffer size in bytes */
#define PICO_RTOS_STREAM_BUFFER_MIN_SIZE 16

/** @brief Maximum message length that can be stored in a stream buffer */
#define PICO_RTOS_STREAM_BUFFER_MAX_MESSAGE_SIZE (UINT32_MAX - sizeof(uint32_t))

/** @brief Threshold for zero-copy optimization (messages larger than this use zero-copy) */
#ifndef PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD
#define PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD 64
#endif

/** @brief Header size for each message (stores message length) */
#define PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE sizeof(uint32_t)

// =============================================================================
// ERROR CODES
// =============================================================================

/** @brief Stream Buffer specific error codes */
/** @brief Stream Buffer specific error codes */
// NOTE: Error codes are now defined in pico_rtos/error.h for global access
// typedef enum {
//     PICO_RTOS_STREAM_BUFFER_ERROR_NONE = 0,
//     PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM = 220,
//     PICO_RTOS_STREAM_BUFFER_ERROR_BUFFER_FULL = 221,
//     PICO_RTOS_STREAM_BUFFER_ERROR_BUFFER_EMPTY = 222,
//     PICO_RTOS_STREAM_BUFFER_ERROR_MESSAGE_TOO_LARGE = 223,
//     PICO_RTOS_STREAM_BUFFER_ERROR_TIMEOUT = 224,
//     PICO_RTOS_STREAM_BUFFER_ERROR_DELETED = 225,
//     PICO_RTOS_STREAM_BUFFER_ERROR_ZERO_COPY_ACTIVE = 226
// } pico_rtos_stream_buffer_error_t;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Stream buffer structure
 * 
 * Implements a circular buffer for variable-length messages with thread-safe
 * operations and blocking support.
 */
typedef struct {
    uint8_t *buffer;                           ///< Circular buffer storage
    uint32_t buffer_size;                      ///< Total buffer size in bytes
    uint32_t head;                             ///< Write position (next write location)
    uint32_t tail;                             ///< Read position (next read location)
    uint32_t bytes_available;                  ///< Current data bytes in buffer
    uint32_t free_space;                       ///< Current free space in buffer
    
    struct pico_rtos_block_object *reader_block_obj;  ///< Reader blocking object
    struct pico_rtos_block_object *writer_block_obj;  ///< Writer blocking object
    
    critical_section_t cs;                     ///< Thread safety critical section
    
    // Zero-copy support
    bool zero_copy_enabled;                    ///< Zero-copy optimization enabled
    void *zero_copy_buffer;                    ///< Zero-copy buffer pointer
    uint32_t zero_copy_size;                   ///< Zero-copy buffer size
    bool zero_copy_active;                     ///< Zero-copy operation in progress
    
    // Statistics and monitoring
    uint32_t messages_sent;                    ///< Total messages sent
    uint32_t messages_received;                ///< Total messages received
    uint32_t bytes_sent;                       ///< Total bytes sent
    uint32_t bytes_received;                   ///< Total bytes received
    uint32_t send_timeouts;                    ///< Number of send timeouts
    uint32_t receive_timeouts;                 ///< Number of receive timeouts
    uint32_t peak_usage;                       ///< Peak buffer usage in bytes
    
    // Configuration flags
    bool wrap_around_enabled;                  ///< Allow buffer wrap-around
    bool overwrite_on_full;                    ///< Overwrite old data when full
} pico_rtos_stream_buffer_t;

/**
 * @brief Zero-copy buffer descriptor
 * 
 * Used for zero-copy operations to provide direct buffer access.
 */
typedef struct {
    void *buffer;                              ///< Direct buffer pointer
    uint32_t size;                             ///< Buffer size
    uint32_t offset;                           ///< Current offset in buffer
    bool read_only;                            ///< Buffer is read-only
} pico_rtos_stream_buffer_zero_copy_t;

/**
 * @brief Stream buffer statistics
 * 
 * Provides detailed statistics for monitoring and debugging.
 */
typedef struct {
    uint32_t buffer_size;                      ///< Total buffer size
    uint32_t bytes_available;                  ///< Current bytes available
    uint32_t free_space;                       ///< Current free space
    uint32_t messages_sent;                    ///< Total messages sent
    uint32_t messages_received;                ///< Total messages received
    uint32_t bytes_sent;                       ///< Total bytes sent
    uint32_t bytes_received;                   ///< Total bytes received
    uint32_t send_timeouts;                    ///< Send timeout count
    uint32_t receive_timeouts;                 ///< Receive timeout count
    uint32_t peak_usage;                       ///< Peak usage in bytes
    uint32_t blocked_senders;                  ///< Currently blocked senders
    uint32_t blocked_receivers;                ///< Currently blocked receivers
    bool zero_copy_enabled;                    ///< Zero-copy enabled
    bool zero_copy_active;                     ///< Zero-copy operation active
} pico_rtos_stream_buffer_stats_t;

// =============================================================================
// CORE API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize a stream buffer
 * 
 * Initializes a stream buffer with the provided storage buffer. The buffer
 * must remain valid for the lifetime of the stream buffer.
 * 
 * @param stream Pointer to stream buffer structure to initialize
 * @param buffer Pointer to storage buffer (must be at least PICO_RTOS_STREAM_BUFFER_MIN_SIZE)
 * @param buffer_size Size of the storage buffer in bytes
 * @return true if initialization successful, false on error
 * 
 * @note The buffer parameter must point to valid memory that remains accessible
 *       for the lifetime of the stream buffer.
 */
bool pico_rtos_stream_buffer_init(pico_rtos_stream_buffer_t *stream, 
                                 uint8_t *buffer, 
                                 uint32_t buffer_size);

/**
 * @brief Send data to a stream buffer
 * 
 * Sends variable-length data to the stream buffer. If the buffer doesn't have
 * enough space, the task will block until space becomes available or timeout occurs.
 * 
 * @param stream Pointer to stream buffer
 * @param data Pointer to data to send
 * @param length Length of data in bytes
 * @param timeout Timeout in milliseconds (PICO_RTOS_WAIT_FOREVER for no timeout)
 * @return Number of bytes actually sent, 0 on timeout or error
 * 
 * @note Messages are stored with a 4-byte header containing the message length,
 *       so the actual buffer space required is length + 4 bytes.
 */
uint32_t pico_rtos_stream_buffer_send(pico_rtos_stream_buffer_t *stream, 
                                     const void *data, 
                                     uint32_t length, 
                                     uint32_t timeout);

/**
 * @brief Receive data from a stream buffer
 * 
 * Receives variable-length data from the stream buffer. If no data is available,
 * the task will block until data arrives or timeout occurs.
 * 
 * @param stream Pointer to stream buffer
 * @param data Pointer to buffer to store received data
 * @param max_length Maximum length of data to receive
 * @param timeout Timeout in milliseconds (PICO_RTOS_WAIT_FOREVER for no timeout)
 * @return Number of bytes actually received, 0 on timeout or error
 * 
 * @note If the received message is larger than max_length, only max_length bytes
 *       will be copied and the rest will be discarded.
 */
uint32_t pico_rtos_stream_buffer_receive(pico_rtos_stream_buffer_t *stream, 
                                        void *data, 
                                        uint32_t max_length, 
                                        uint32_t timeout);

/**
 * @brief Delete a stream buffer and free associated resources
 * 
 * Deletes the stream buffer and unblocks all waiting tasks. The storage buffer
 * provided during initialization is not freed and remains the caller's responsibility.
 * 
 * @param stream Pointer to stream buffer to delete
 */
void pico_rtos_stream_buffer_delete(pico_rtos_stream_buffer_t *stream);

// =============================================================================
// QUERY AND STATUS FUNCTIONS
// =============================================================================

/**
 * @brief Check if stream buffer is empty
 * 
 * @param stream Pointer to stream buffer
 * @return true if buffer is empty, false otherwise
 */
bool pico_rtos_stream_buffer_is_empty(pico_rtos_stream_buffer_t *stream);

/**
 * @brief Check if stream buffer is full
 * 
 * @param stream Pointer to stream buffer
 * @return true if buffer is full, false otherwise
 */
bool pico_rtos_stream_buffer_is_full(pico_rtos_stream_buffer_t *stream);

/**
 * @brief Get number of bytes available for reading
 * 
 * @param stream Pointer to stream buffer
 * @return Number of bytes available for reading
 */
uint32_t pico_rtos_stream_buffer_bytes_available(pico_rtos_stream_buffer_t *stream);

/**
 * @brief Get amount of free space available for writing
 * 
 * @param stream Pointer to stream buffer
 * @return Number of bytes of free space available
 */
uint32_t pico_rtos_stream_buffer_free_space(pico_rtos_stream_buffer_t *stream);

/**
 * @brief Get the next message length without removing it
 * 
 * Peeks at the next message in the buffer to determine its length without
 * actually receiving the message.
 * 
 * @param stream Pointer to stream buffer
 * @return Length of next message in bytes, 0 if buffer is empty
 */
uint32_t pico_rtos_stream_buffer_peek_message_length(pico_rtos_stream_buffer_t *stream);

// =============================================================================
// ZERO-COPY API FUNCTIONS
// =============================================================================

/**
 * @brief Enable zero-copy optimization
 * 
 * Enables zero-copy semantics for messages larger than the configured threshold.
 * This can improve performance for large data transfers by avoiding memory copies.
 * 
 * @param stream Pointer to stream buffer
 * @param enable true to enable zero-copy, false to disable
 * @return true if successful, false if zero-copy operation is currently active
 */
bool pico_rtos_stream_buffer_set_zero_copy(pico_rtos_stream_buffer_t *stream, bool enable);

/**
 * @brief Get direct buffer access for zero-copy send
 * 
 * Provides direct access to the stream buffer for zero-copy send operations.
 * The caller must call pico_rtos_stream_buffer_zero_copy_send_complete() when done.
 * 
 * @param stream Pointer to stream buffer
 * @param length Required buffer length
 * @param zero_copy Pointer to zero-copy descriptor to fill
 * @param timeout Timeout in milliseconds
 * @return true if buffer access granted, false on timeout or error
 */
bool pico_rtos_stream_buffer_zero_copy_send_start(pico_rtos_stream_buffer_t *stream,
                                                 uint32_t length,
                                                 pico_rtos_stream_buffer_zero_copy_t *zero_copy,
                                                 uint32_t timeout);

/**
 * @brief Complete zero-copy send operation
 * 
 * Completes a zero-copy send operation started with pico_rtos_stream_buffer_zero_copy_send_start().
 * 
 * @param stream Pointer to stream buffer
 * @param zero_copy Pointer to zero-copy descriptor
 * @param actual_length Actual number of bytes written
 * @return true if successful, false on error
 */
bool pico_rtos_stream_buffer_zero_copy_send_complete(pico_rtos_stream_buffer_t *stream,
                                                    pico_rtos_stream_buffer_zero_copy_t *zero_copy,
                                                    uint32_t actual_length);

/**
 * @brief Get direct buffer access for zero-copy receive
 * 
 * Provides direct access to the stream buffer for zero-copy receive operations.
 * The caller must call pico_rtos_stream_buffer_zero_copy_receive_complete() when done.
 * 
 * @param stream Pointer to stream buffer
 * @param zero_copy Pointer to zero-copy descriptor to fill
 * @param timeout Timeout in milliseconds
 * @return true if data available, false on timeout or error
 */
bool pico_rtos_stream_buffer_zero_copy_receive_start(pico_rtos_stream_buffer_t *stream,
                                                    pico_rtos_stream_buffer_zero_copy_t *zero_copy,
                                                    uint32_t timeout);

/**
 * @brief Complete zero-copy receive operation
 * 
 * Completes a zero-copy receive operation started with pico_rtos_stream_buffer_zero_copy_receive_start().
 * 
 * @param stream Pointer to stream buffer
 * @param zero_copy Pointer to zero-copy descriptor
 * @return true if successful, false on error
 */
bool pico_rtos_stream_buffer_zero_copy_receive_complete(pico_rtos_stream_buffer_t *stream,
                                                       pico_rtos_stream_buffer_zero_copy_t *zero_copy);

// =============================================================================
// STATISTICS AND MONITORING
// =============================================================================

/**
 * @brief Get stream buffer statistics
 * 
 * Retrieves detailed statistics about the stream buffer for monitoring and debugging.
 * 
 * @param stream Pointer to stream buffer
 * @param stats Pointer to statistics structure to fill
 * @return true if successful, false on error
 */
bool pico_rtos_stream_buffer_get_stats(pico_rtos_stream_buffer_t *stream,
                                      pico_rtos_stream_buffer_stats_t *stats);

/**
 * @brief Reset stream buffer statistics
 * 
 * Resets all statistics counters to zero while preserving current buffer state.
 * 
 * @param stream Pointer to stream buffer
 */
void pico_rtos_stream_buffer_reset_stats(pico_rtos_stream_buffer_t *stream);

// =============================================================================
// ADVANCED CONFIGURATION
// =============================================================================

/**
 * @brief Set buffer wrap-around behavior
 * 
 * Controls whether the buffer allows wrap-around when writing data.
 * 
 * @param stream Pointer to stream buffer
 * @param enable true to enable wrap-around, false to disable
 */
void pico_rtos_stream_buffer_set_wrap_around(pico_rtos_stream_buffer_t *stream, bool enable);

/**
 * @brief Set overwrite behavior when buffer is full
 * 
 * Controls whether new data overwrites old data when the buffer is full.
 * 
 * @param stream Pointer to stream buffer
 * @param enable true to enable overwrite, false to block when full
 */
void pico_rtos_stream_buffer_set_overwrite_on_full(pico_rtos_stream_buffer_t *stream, bool enable);

/**
 * @brief Flush all data from the stream buffer
 * 
 * Removes all data from the buffer and unblocks any waiting receivers.
 * 
 * @param stream Pointer to stream buffer
 */
void pico_rtos_stream_buffer_flush(pico_rtos_stream_buffer_t *stream);

#endif // PICO_RTOS_STREAM_BUFFER_H