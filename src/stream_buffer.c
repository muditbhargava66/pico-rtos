#include "pico_rtos/stream_buffer.h"
#include "pico_rtos/blocking.h"
#include "pico_rtos/task.h"
#include "pico_rtos.h"
#include "pico/critical_section.h"
#include <string.h>
#include <stdlib.h>

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Calculate circular buffer space accounting for wrap-around
 * 
 * @param head Current head position
 * @param tail Current tail position  
 * @param buffer_size Total buffer size
 * @return Available space in bytes
 */
static inline uint32_t calculate_free_space(uint32_t head, uint32_t tail, uint32_t buffer_size) {
    if (head >= tail) {
        return buffer_size - (head - tail) - 1; // -1 to distinguish full from empty
    } else {
        return tail - head - 1;
    }
}

/**
 * @brief Calculate available data in circular buffer
 * 
 * @param head Current head position
 * @param tail Current tail position
 * @param buffer_size Total buffer size
 * @return Available data in bytes
 */
static inline uint32_t calculate_available_data(uint32_t head, uint32_t tail, uint32_t buffer_size) {
    if (head >= tail) {
        return head - tail;
    } else {
        return buffer_size - (tail - head);
    }
}

/**
 * @brief Write data to circular buffer with wrap-around handling
 * 
 * @param buffer Circular buffer
 * @param buffer_size Buffer size
 * @param position Current write position
 * @param data Data to write
 * @param length Length of data
 * @return New write position
 */
static uint32_t write_to_circular_buffer(uint8_t *buffer, uint32_t buffer_size, 
                                        uint32_t position, const void *data, uint32_t length) {
    const uint8_t *src = (const uint8_t *)data;
    uint32_t remaining = length;
    uint32_t current_pos = position;
    
    while (remaining > 0) {
        uint32_t chunk_size = remaining;
        uint32_t space_to_end = buffer_size - current_pos;
        
        if (chunk_size > space_to_end) {
            chunk_size = space_to_end;
        }
        
        memcpy(&buffer[current_pos], src, chunk_size);
        
        src += chunk_size;
        remaining -= chunk_size;
        current_pos = (current_pos + chunk_size) % buffer_size;
    }
    
    return current_pos;
}

/**
 * @brief Read data from circular buffer with wrap-around handling
 * 
 * @param buffer Circular buffer
 * @param buffer_size Buffer size
 * @param position Current read position
 * @param data Buffer to store read data
 * @param length Length of data to read
 * @return New read position
 */
static uint32_t read_from_circular_buffer(uint8_t *buffer, uint32_t buffer_size,
                                         uint32_t position, void *data, uint32_t length) {
    uint8_t *dest = (uint8_t *)data;
    uint32_t remaining = length;
    uint32_t current_pos = position;
    
    while (remaining > 0) {
        uint32_t chunk_size = remaining;
        uint32_t data_to_end = buffer_size - current_pos;
        
        if (chunk_size > data_to_end) {
            chunk_size = data_to_end;
        }
        
        memcpy(dest, &buffer[current_pos], chunk_size);
        
        dest += chunk_size;
        remaining -= chunk_size;
        current_pos = (current_pos + chunk_size) % buffer_size;
    }
    
    return current_pos;
}

// =============================================================================
// CORE API IMPLEMENTATION
// =============================================================================

bool pico_rtos_stream_buffer_init(pico_rtos_stream_buffer_t *stream, 
                                 uint8_t *buffer, 
                                 uint32_t buffer_size) {
    // Parameter validation
    if (stream == NULL || buffer == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (buffer_size < PICO_RTOS_STREAM_BUFFER_MIN_SIZE) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, buffer_size);
        return false;
    }
    
    // Initialize basic buffer state
    stream->buffer = buffer;
    stream->buffer_size = buffer_size;
    stream->head = 0;
    stream->tail = 0;
    stream->bytes_available = 0;
    stream->free_space = buffer_size - 1; // -1 to distinguish full from empty
    
    // Initialize critical section for thread safety
    critical_section_init(&stream->cs);
    
    // Create blocking objects for reader and writer synchronization
    stream->reader_block_obj = pico_rtos_block_object_create(stream);
    stream->writer_block_obj = pico_rtos_block_object_create(stream);
    
    if (stream->reader_block_obj == NULL || stream->writer_block_obj == NULL) {
        // Cleanup on failure
        if (stream->reader_block_obj != NULL) {
            pico_rtos_block_object_delete(stream->reader_block_obj);
        }
        if (stream->writer_block_obj != NULL) {
            pico_rtos_block_object_delete(stream->writer_block_obj);
        }
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_OUT_OF_MEMORY, 0);
        return false;
    }
    
    // Initialize zero-copy support
    stream->zero_copy_enabled = false;
    stream->zero_copy_buffer = NULL;
    stream->zero_copy_size = 0;
    stream->zero_copy_active = false;
    
    // Initialize statistics
    stream->messages_sent = 0;
    stream->messages_received = 0;
    stream->bytes_sent = 0;
    stream->bytes_received = 0;
    stream->send_timeouts = 0;
    stream->receive_timeouts = 0;
    stream->peak_usage = 0;
    
    // Initialize configuration flags
    stream->wrap_around_enabled = true;
    stream->overwrite_on_full = false;
    
    return true;
}

uint32_t pico_rtos_stream_buffer_send(pico_rtos_stream_buffer_t *stream, 
                                     const void *data, 
                                     uint32_t length, 
                                     uint32_t timeout) {
    // Parameter validation
    if (stream == NULL || data == NULL || length == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return 0;
    }
    
    if (length > PICO_RTOS_STREAM_BUFFER_MAX_MESSAGE_SIZE) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_MESSAGE_TOO_LARGE, length);
        return 0;
    }
    
    // Calculate total space needed (message length header + data)
    uint32_t total_size = PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + length;
    
    critical_section_enter_blocking(&stream->cs);
    
    // Check if we have enough space
    while (stream->free_space < total_size) {
        // If no timeout, return immediately
        if (timeout == PICO_RTOS_NO_WAIT) {
            critical_section_exit(&stream->cs);
            return 0;
        }
        
        // Block current task until space becomes available
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (current_task == NULL) {
            critical_section_exit(&stream->cs);
            return 0;
        }
        
        critical_section_exit(&stream->cs);
        
        // Block on writer blocking object
        bool blocked = pico_rtos_block_task(stream->writer_block_obj, current_task, 
                                          PICO_RTOS_BLOCK_REASON_QUEUE_FULL, timeout);
        if (!blocked) {
            stream->send_timeouts++;
            return 0;
        }
        
        // Trigger scheduler to switch tasks
        pico_rtos_scheduler();
        
        // Check if we timed out
        if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
            stream->send_timeouts++;
            return 0;
        }
        
        // Reacquire lock and check space again
        critical_section_enter_blocking(&stream->cs);
    }
    
    // At this point we have enough space - write the message
    
    // Write message length header
    uint32_t new_head = write_to_circular_buffer(stream->buffer, stream->buffer_size,
                                                stream->head, &length, sizeof(uint32_t));
    
    // Write message data
    new_head = write_to_circular_buffer(stream->buffer, stream->buffer_size,
                                       new_head, data, length);
    
    // Update buffer state
    stream->head = new_head;
    stream->bytes_available += total_size;
    stream->free_space -= total_size;
    
    // Update statistics
    stream->messages_sent++;
    stream->bytes_sent += length;
    if (stream->bytes_available > stream->peak_usage) {
        stream->peak_usage = stream->bytes_available;
    }
    
    // Unblock any waiting readers
    pico_rtos_task_t *unblocked_reader = pico_rtos_unblock_highest_priority_task(stream->reader_block_obj);
    if (unblocked_reader != NULL) {
        // Task is automatically made ready by the unblock function
        pico_rtos_request_context_switch();
    }
    
    critical_section_exit(&stream->cs);
    
    return length;
}

uint32_t pico_rtos_stream_buffer_receive(pico_rtos_stream_buffer_t *stream, 
                                        void *data, 
                                        uint32_t max_length, 
                                        uint32_t timeout) {
    // Parameter validation
    if (stream == NULL || data == NULL || max_length == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return 0;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Check if we have any data available
    while (stream->bytes_available < PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE) {
        // If no timeout, return immediately
        if (timeout == PICO_RTOS_NO_WAIT) {
            critical_section_exit(&stream->cs);
            return 0;
        }
        
        // Block current task until data becomes available
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (current_task == NULL) {
            critical_section_exit(&stream->cs);
            return 0;
        }
        
        critical_section_exit(&stream->cs);
        
        // Block on reader blocking object
        bool blocked = pico_rtos_block_task(stream->reader_block_obj, current_task,
                                          PICO_RTOS_BLOCK_REASON_QUEUE_EMPTY, timeout);
        if (!blocked) {
            stream->receive_timeouts++;
            return 0;
        }
        
        // Trigger scheduler to switch tasks
        pico_rtos_scheduler();
        
        // Check if we timed out
        if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
            stream->receive_timeouts++;
            return 0;
        }
        
        // Reacquire lock and check data again
        critical_section_enter_blocking(&stream->cs);
    }
    
    // At this point we have at least a message header - read the message length
    uint32_t message_length;
    uint32_t new_tail = read_from_circular_buffer(stream->buffer, stream->buffer_size,
                                                 stream->tail, &message_length, sizeof(uint32_t));
    
    // Validate message length
    if (message_length > PICO_RTOS_STREAM_BUFFER_MAX_MESSAGE_SIZE ||
        stream->bytes_available < (PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + message_length)) {
        // Corrupted message - flush buffer to recover
        stream->head = 0;
        stream->tail = 0;
        stream->bytes_available = 0;
        stream->free_space = stream->buffer_size - 1;
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_CORRUPTION, message_length);
        return 0;
    }
    
    // Determine how much data to actually copy
    uint32_t bytes_to_copy = (message_length <= max_length) ? message_length : max_length;
    
    // Read message data
    new_tail = read_from_circular_buffer(stream->buffer, stream->buffer_size,
                                        new_tail, data, bytes_to_copy);
    
    // If message was larger than buffer, skip the remaining data
    if (message_length > max_length) {
        uint32_t bytes_to_skip = message_length - max_length;
        new_tail = (new_tail + bytes_to_skip) % stream->buffer_size;
    }
    
    // Update buffer state
    stream->tail = new_tail;
    uint32_t total_message_size = PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + message_length;
    stream->bytes_available -= total_message_size;
    stream->free_space += total_message_size;
    
    // Update statistics
    stream->messages_received++;
    stream->bytes_received += bytes_to_copy;
    
    // Unblock any waiting writers
    pico_rtos_task_t *unblocked_writer = pico_rtos_unblock_highest_priority_task(stream->writer_block_obj);
    if (unblocked_writer != NULL) {
        // Task is automatically made ready by the unblock function
        pico_rtos_request_context_switch();
    }
    
    critical_section_exit(&stream->cs);
    
    return bytes_to_copy;
}

void pico_rtos_stream_buffer_delete(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Delete blocking objects (this will unblock all waiting tasks)
    if (stream->reader_block_obj != NULL) {
        pico_rtos_block_object_delete(stream->reader_block_obj);
        stream->reader_block_obj = NULL;
    }
    
    if (stream->writer_block_obj != NULL) {
        pico_rtos_block_object_delete(stream->writer_block_obj);
        stream->writer_block_obj = NULL;
    }
    
    // Reset buffer state
    stream->head = 0;
    stream->tail = 0;
    stream->bytes_available = 0;
    stream->free_space = 0;
    stream->zero_copy_active = false;
    
    critical_section_exit(&stream->cs);
}

// =============================================================================
// QUERY AND STATUS FUNCTIONS
// =============================================================================

bool pico_rtos_stream_buffer_is_empty(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return true;
    }
    
    critical_section_enter_blocking(&stream->cs);
    bool is_empty = (stream->bytes_available == 0);
    critical_section_exit(&stream->cs);
    
    return is_empty;
}

bool pico_rtos_stream_buffer_is_full(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return true;
    }
    
    critical_section_enter_blocking(&stream->cs);
    bool is_full = (stream->free_space < PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + 1);
    critical_section_exit(&stream->cs);
    
    return is_full;
}

uint32_t pico_rtos_stream_buffer_bytes_available(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&stream->cs);
    uint32_t available = stream->bytes_available;
    critical_section_exit(&stream->cs);
    
    return available;
}

uint32_t pico_rtos_stream_buffer_free_space(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&stream->cs);
    uint32_t free_space = stream->free_space;
    critical_section_exit(&stream->cs);
    
    return free_space;
}

uint32_t pico_rtos_stream_buffer_peek_message_length(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Check if we have at least a message header
    if (stream->bytes_available < PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE) {
        critical_section_exit(&stream->cs);
        return 0;
    }
    
    // Read message length without advancing tail
    uint32_t message_length;
    read_from_circular_buffer(stream->buffer, stream->buffer_size,
                             stream->tail, &message_length, sizeof(uint32_t));
    
    critical_section_exit(&stream->cs);
    
    return message_length;
}

// =============================================================================
// STATISTICS AND MONITORING
// =============================================================================

bool pico_rtos_stream_buffer_get_stats(pico_rtos_stream_buffer_t *stream,
                                      pico_rtos_stream_buffer_stats_t *stats) {
    if (stream == NULL || stats == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    stats->buffer_size = stream->buffer_size;
    stats->bytes_available = stream->bytes_available;
    stats->free_space = stream->free_space;
    stats->messages_sent = stream->messages_sent;
    stats->messages_received = stream->messages_received;
    stats->bytes_sent = stream->bytes_sent;
    stats->bytes_received = stream->bytes_received;
    stats->send_timeouts = stream->send_timeouts;
    stats->receive_timeouts = stream->receive_timeouts;
    stats->peak_usage = stream->peak_usage;
    stats->blocked_senders = pico_rtos_get_blocked_count(stream->writer_block_obj);
    stats->blocked_receivers = pico_rtos_get_blocked_count(stream->reader_block_obj);
    stats->zero_copy_enabled = stream->zero_copy_enabled;
    stats->zero_copy_active = stream->zero_copy_active;
    
    critical_section_exit(&stream->cs);
    
    return true;
}

void pico_rtos_stream_buffer_reset_stats(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    stream->messages_sent = 0;
    stream->messages_received = 0;
    stream->bytes_sent = 0;
    stream->bytes_received = 0;
    stream->send_timeouts = 0;
    stream->receive_timeouts = 0;
    stream->peak_usage = stream->bytes_available; // Reset to current usage
    
    critical_section_exit(&stream->cs);
}

// =============================================================================
// ADVANCED CONFIGURATION
// =============================================================================

void pico_rtos_stream_buffer_set_wrap_around(pico_rtos_stream_buffer_t *stream, bool enable) {
    if (stream == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&stream->cs);
    stream->wrap_around_enabled = enable;
    critical_section_exit(&stream->cs);
}

void pico_rtos_stream_buffer_set_overwrite_on_full(pico_rtos_stream_buffer_t *stream, bool enable) {
    if (stream == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&stream->cs);
    stream->overwrite_on_full = enable;
    critical_section_exit(&stream->cs);
}

void pico_rtos_stream_buffer_flush(pico_rtos_stream_buffer_t *stream) {
    if (stream == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Reset buffer to empty state
    stream->head = 0;
    stream->tail = 0;
    stream->bytes_available = 0;
    stream->free_space = stream->buffer_size - 1;
    
    // Unblock all waiting receivers (they will get 0 bytes)
    pico_rtos_task_t *unblocked_reader;
    while ((unblocked_reader = pico_rtos_unblock_highest_priority_task(stream->reader_block_obj)) != NULL) {
        // Task is automatically made ready by the unblock function
    }
    
    // Unblock all waiting writers (they now have space)
    pico_rtos_task_t *unblocked_writer;
    while ((unblocked_writer = pico_rtos_unblock_highest_priority_task(stream->writer_block_obj)) != NULL) {
        // Task is automatically made ready by the unblock function
    }
    
    // Request context switch if any tasks were unblocked
    pico_rtos_request_context_switch();
    
    critical_section_exit(&stream->cs);
}

// =============================================================================
// ZERO-COPY API IMPLEMENTATION (Basic stubs for subtask 3.3)
// =============================================================================

bool pico_rtos_stream_buffer_set_zero_copy(pico_rtos_stream_buffer_t *stream, bool enable) {
    if (stream == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Cannot change zero-copy setting while operation is active
    if (stream->zero_copy_active) {
        critical_section_exit(&stream->cs);
        return false;
    }
    
    stream->zero_copy_enabled = enable;
    critical_section_exit(&stream->cs);
    
    return true;
}

// =============================================================================
// ZERO-COPY API IMPLEMENTATION
// =============================================================================

bool pico_rtos_stream_buffer_zero_copy_send_start(pico_rtos_stream_buffer_t *stream,
                                                 uint32_t length,
                                                 pico_rtos_stream_buffer_zero_copy_t *zero_copy,
                                                 uint32_t timeout) {
    // Parameter validation
    if (stream == NULL || zero_copy == NULL || length == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (!stream->zero_copy_enabled) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, 0);
        return false;
    }
    
    if (length > PICO_RTOS_STREAM_BUFFER_MAX_MESSAGE_SIZE) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_MESSAGE_TOO_LARGE, length);
        return false;
    }
    
    // Only use zero-copy for messages above threshold
    if (length < PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, length);
        return false;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Check if zero-copy operation is already active
    if (stream->zero_copy_active) {
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_ZERO_COPY_ACTIVE, 0);
        return false;
    }
    
    // Calculate total space needed (message length header + data)
    uint32_t total_size = PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + length;
    
    // Check if we have enough contiguous space
    while (stream->free_space < total_size) {
        // If no timeout, return immediately
        if (timeout == PICO_RTOS_NO_WAIT) {
            critical_section_exit(&stream->cs);
            return false;
        }
        
        // Block current task until space becomes available
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (current_task == NULL) {
            critical_section_exit(&stream->cs);
            return false;
        }
        
        critical_section_exit(&stream->cs);
        
        // Block on writer blocking object
        bool blocked = pico_rtos_block_task(stream->writer_block_obj, current_task, 
                                          PICO_RTOS_BLOCK_REASON_QUEUE_FULL, timeout);
        if (!blocked) {
            stream->send_timeouts++;
            return false;
        }
        
        // Trigger scheduler to switch tasks
        pico_rtos_scheduler();
        
        // Check if we timed out
        if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
            stream->send_timeouts++;
            return false;
        }
        
        // Reacquire lock and check space again
        critical_section_enter_blocking(&stream->cs);
    }
    
    // Check if we have enough contiguous space from head to end of buffer
    uint32_t space_to_end = stream->buffer_size - stream->head;
    bool needs_wrap = (total_size > space_to_end);
    
    if (needs_wrap && !stream->wrap_around_enabled) {
        // Cannot provide contiguous space without wrap-around
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_BUFFER_FULL, total_size);
        return false;
    }
    
    // If we need to wrap and there's not enough space at the beginning
    if (needs_wrap && (stream->tail < total_size)) {
        // Not enough contiguous space available
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_BUFFER_FULL, total_size);
        return false;
    }
    
    // Set up zero-copy operation
    stream->zero_copy_active = true;
    stream->zero_copy_size = length;
    
    // Write message length header first
    write_to_circular_buffer(stream->buffer, stream->buffer_size,
                            stream->head, &length, sizeof(uint32_t));
    
    // Provide direct buffer access for data
    uint32_t data_start_pos = (stream->head + PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE) % stream->buffer_size;
    
    if (needs_wrap) {
        // Wrap to beginning of buffer
        zero_copy->buffer = &stream->buffer[0];
        zero_copy->size = length;
        zero_copy->offset = 0;
        stream->zero_copy_buffer = &stream->buffer[0];
    } else {
        // Contiguous space available
        zero_copy->buffer = &stream->buffer[data_start_pos];
        zero_copy->size = length;
        zero_copy->offset = 0;
        stream->zero_copy_buffer = &stream->buffer[data_start_pos];
    }
    
    zero_copy->read_only = false;
    
    critical_section_exit(&stream->cs);
    
    return true;
}

bool pico_rtos_stream_buffer_zero_copy_send_complete(pico_rtos_stream_buffer_t *stream,
                                                    pico_rtos_stream_buffer_zero_copy_t *zero_copy,
                                                    uint32_t actual_length) {
    // Parameter validation
    if (stream == NULL || zero_copy == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Verify zero-copy operation is active
    if (!stream->zero_copy_active || stream->zero_copy_buffer != zero_copy->buffer) {
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, 0);
        return false;
    }
    
    // Validate actual length
    if (actual_length > stream->zero_copy_size) {
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_MESSAGE_TOO_LARGE, actual_length);
        return false;
    }
    
    // Update message length header if actual length is different
    if (actual_length != stream->zero_copy_size) {
        write_to_circular_buffer(stream->buffer, stream->buffer_size,
                                stream->head, &actual_length, sizeof(uint32_t));
    }
    
    // Update buffer state
    uint32_t total_size = PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + actual_length;
    stream->head = (stream->head + total_size) % stream->buffer_size;
    stream->bytes_available += total_size;
    stream->free_space -= total_size;
    
    // Update statistics
    stream->messages_sent++;
    stream->bytes_sent += actual_length;
    if (stream->bytes_available > stream->peak_usage) {
        stream->peak_usage = stream->bytes_available;
    }
    
    // Clear zero-copy state
    stream->zero_copy_active = false;
    stream->zero_copy_buffer = NULL;
    stream->zero_copy_size = 0;
    
    // Unblock any waiting readers
    pico_rtos_task_t *unblocked_reader = pico_rtos_unblock_highest_priority_task(stream->reader_block_obj);
    if (unblocked_reader != NULL) {
        pico_rtos_request_context_switch();
    }
    
    critical_section_exit(&stream->cs);
    
    return true;
}

bool pico_rtos_stream_buffer_zero_copy_receive_start(pico_rtos_stream_buffer_t *stream,
                                                    pico_rtos_stream_buffer_zero_copy_t *zero_copy,
                                                    uint32_t timeout) {
    // Parameter validation
    if (stream == NULL || zero_copy == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (!stream->zero_copy_enabled) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, 0);
        return false;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Check if zero-copy operation is already active
    if (stream->zero_copy_active) {
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_ZERO_COPY_ACTIVE, 0);
        return false;
    }
    
    // Check if we have any data available
    while (stream->bytes_available < PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE) {
        // If no timeout, return immediately
        if (timeout == PICO_RTOS_NO_WAIT) {
            critical_section_exit(&stream->cs);
            return false;
        }
        
        // Block current task until data becomes available
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (current_task == NULL) {
            critical_section_exit(&stream->cs);
            return false;
        }
        
        critical_section_exit(&stream->cs);
        
        // Block on reader blocking object
        bool blocked = pico_rtos_block_task(stream->reader_block_obj, current_task,
                                          PICO_RTOS_BLOCK_REASON_QUEUE_EMPTY, timeout);
        if (!blocked) {
            stream->receive_timeouts++;
            return false;
        }
        
        // Trigger scheduler to switch tasks
        pico_rtos_scheduler();
        
        // Check if we timed out
        if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
            stream->receive_timeouts++;
            return false;
        }
        
        // Reacquire lock and check data again
        critical_section_enter_blocking(&stream->cs);
    }
    
    // Read message length
    uint32_t message_length;
    read_from_circular_buffer(stream->buffer, stream->buffer_size,
                             stream->tail, &message_length, sizeof(uint32_t));
    
    // Validate message length
    if (message_length > PICO_RTOS_STREAM_BUFFER_MAX_MESSAGE_SIZE ||
        stream->bytes_available < (PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + message_length)) {
        // Corrupted message - flush buffer to recover
        stream->head = 0;
        stream->tail = 0;
        stream->bytes_available = 0;
        stream->free_space = stream->buffer_size - 1;
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_CORRUPTION, message_length);
        return false;
    }
    
    // Only use zero-copy for messages above threshold
    if (message_length < PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD) {
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, message_length);
        return false;
    }
    
    // Set up zero-copy operation
    stream->zero_copy_active = true;
    stream->zero_copy_size = message_length;
    
    // Provide direct buffer access for data
    uint32_t data_start_pos = (stream->tail + PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE) % stream->buffer_size;
    uint32_t space_to_end = stream->buffer_size - data_start_pos;
    
    if (message_length > space_to_end) {
        // Message wraps around - for zero-copy, we need contiguous access
        // This is a limitation of the zero-copy implementation
        stream->zero_copy_active = false;
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, message_length);
        return false;
    }
    
    // Provide direct buffer access
    zero_copy->buffer = &stream->buffer[data_start_pos];
    zero_copy->size = message_length;
    zero_copy->offset = 0;
    zero_copy->read_only = true;
    stream->zero_copy_buffer = &stream->buffer[data_start_pos];
    
    critical_section_exit(&stream->cs);
    
    return true;
}

bool pico_rtos_stream_buffer_zero_copy_receive_complete(pico_rtos_stream_buffer_t *stream,
                                                       pico_rtos_stream_buffer_zero_copy_t *zero_copy) {
    // Parameter validation
    if (stream == NULL || zero_copy == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    critical_section_enter_blocking(&stream->cs);
    
    // Verify zero-copy operation is active
    if (!stream->zero_copy_active || stream->zero_copy_buffer != zero_copy->buffer) {
        critical_section_exit(&stream->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM, 0);
        return false;
    }
    
    // Update buffer state
    uint32_t total_message_size = PICO_RTOS_STREAM_BUFFER_MESSAGE_HEADER_SIZE + stream->zero_copy_size;
    stream->tail = (stream->tail + total_message_size) % stream->buffer_size;
    stream->bytes_available -= total_message_size;
    stream->free_space += total_message_size;
    
    // Update statistics
    stream->messages_received++;
    stream->bytes_received += stream->zero_copy_size;
    
    // Clear zero-copy state
    stream->zero_copy_active = false;
    stream->zero_copy_buffer = NULL;
    stream->zero_copy_size = 0;
    
    // Unblock any waiting writers
    pico_rtos_task_t *unblocked_writer = pico_rtos_unblock_highest_priority_task(stream->writer_block_obj);
    if (unblocked_writer != NULL) {
        pico_rtos_request_context_switch();
    }
    
    critical_section_exit(&stream->cs);
    
    return true;
}