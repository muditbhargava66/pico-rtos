#include "pico_rtos/trace.h"
#include "pico_rtos/config.h"
#include "pico_rtos.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if PICO_RTOS_ENABLE_SYSTEM_TRACING

// =============================================================================
// INTERNAL CONSTANTS AND MACROS
// =============================================================================

#define TRACE_MAGIC_NUMBER 0x54524143  ///< "TRAC" in hex
#define INVALID_EVENT_INDEX 0xFFFFFFFF ///< Invalid event index marker

// =============================================================================
// INTERNAL VARIABLES
// =============================================================================

static pico_rtos_trace_buffer_t trace_buffer = {0};
static bool trace_initialized = false;

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current time in microseconds with high resolution
 */
static inline uint64_t get_time_us(void) {
    return time_us_64();
}

/**
 * @brief Check if an event type should be recorded based on filters
 */
static bool should_record_event(pico_rtos_trace_event_type_t type, 
                               pico_rtos_trace_priority_t priority,
                               uint32_t task_id) {
    if (!trace_buffer.tracing_enabled) {
        return false;
    }
    
    // Check type filter
    if (trace_buffer.filter_mask != 0) {
        uint32_t type_bit = 1U << (uint32_t)type;
        if ((trace_buffer.filter_mask & type_bit) == 0) {
            return false;
        }
    }
    
    // Check priority filter
    if (priority < trace_buffer.min_priority) {
        return false;
    }
    
    // Check task filter (0 means all tasks)
    // Note: This would need to be implemented with actual task filtering
    // For now, we'll skip this check
    (void)task_id;
    
    return true;
}

/**
 * @brief Get the next write position in the circular buffer
 */
static uint32_t get_next_write_position(void) {
    if (!trace_buffer.events) {
        return INVALID_EVENT_INDEX;
    }
    
    uint32_t next_pos = trace_buffer.head;
    
    if (trace_buffer.overflow_behavior == PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP) {
        // Always allow writing, wrap around if necessary
        return next_pos;
    } else {
        // Stop writing when buffer is full
        if (trace_buffer.buffer_full) {
            return INVALID_EVENT_INDEX;
        }
        return next_pos;
    }
}

/**
 * @brief Advance the write position and update buffer state
 */
static void advance_write_position(void) {
    if (!trace_buffer.events) {
        return;
    }
    
    trace_buffer.head = (trace_buffer.head + 1) % trace_buffer.buffer_size;
    trace_buffer.event_count++;
    
    // Check if buffer is full
    if (trace_buffer.overflow_behavior == PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP) {
        // In wrap mode, adjust tail if we're overwriting
        if (trace_buffer.event_count > trace_buffer.buffer_size) {
            trace_buffer.tail = (trace_buffer.tail + 1) % trace_buffer.buffer_size;
            trace_buffer.event_count = trace_buffer.buffer_size;
        }
    } else {
        // In stop mode, mark buffer as full when we reach capacity
        if (trace_buffer.head == trace_buffer.tail && trace_buffer.event_count > 0) {
            trace_buffer.buffer_full = true;
        }
    }
}

/**
 * @brief Calculate buffer utilization percentage
 */
static uint32_t calculate_buffer_utilization(void) {
    if (trace_buffer.buffer_size == 0) {
        return 0;
    }
    
    uint32_t used_events = (trace_buffer.event_count < trace_buffer.buffer_size) ? 
                          trace_buffer.event_count : trace_buffer.buffer_size;
    
    return (used_events * 100) / trace_buffer.buffer_size;
}

// =============================================================================
// TRACE BUFFER MANAGEMENT IMPLEMENTATION
// =============================================================================

bool pico_rtos_trace_init(uint32_t buffer_size, pico_rtos_trace_overflow_behavior_t overflow_behavior) {
    if (trace_initialized) {
        return false; // Already initialized
    }
    
    if (buffer_size == 0 || buffer_size > PICO_RTOS_TRACE_BUFFER_SIZE * 4) {
        buffer_size = PICO_RTOS_TRACE_BUFFER_SIZE;
    }
    
    // Allocate memory for trace events
    pico_rtos_trace_event_t *events = (pico_rtos_trace_event_t *)pico_rtos_malloc(
        buffer_size * sizeof(pico_rtos_trace_event_t));
    
    if (!events) {
        return false;
    }
    
    // Initialize trace buffer structure
    memset(&trace_buffer, 0, sizeof(pico_rtos_trace_buffer_t));
    trace_buffer.events = events;
    trace_buffer.buffer_size = buffer_size;
    trace_buffer.overflow_behavior = overflow_behavior;
    trace_buffer.tracing_enabled = true;
    trace_buffer.filter_mask = 0; // Record all event types by default
    trace_buffer.min_priority = PICO_RTOS_TRACE_PRIORITY_LOW;
    
    // Clear all events
    memset(events, 0, buffer_size * sizeof(pico_rtos_trace_event_t));
    
    trace_initialized = true;
    return true;
}

void pico_rtos_trace_deinit(void) {
    if (!trace_initialized) {
        return;
    }
    
    trace_buffer.tracing_enabled = false;
    
    if (trace_buffer.events) {
        pico_rtos_free(trace_buffer.events, trace_buffer.buffer_size * sizeof(pico_rtos_trace_event_t));
        trace_buffer.events = NULL;
    }
    
    memset(&trace_buffer, 0, sizeof(pico_rtos_trace_buffer_t));
    trace_initialized = false;
}

void pico_rtos_trace_enable(bool enable) {
    if (trace_initialized) {
        trace_buffer.tracing_enabled = enable;
    }
}

bool pico_rtos_trace_is_enabled(void) {
    return trace_initialized && trace_buffer.tracing_enabled;
}

void pico_rtos_trace_clear(void) {
    if (!trace_initialized || !trace_buffer.events) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    // Reset buffer pointers and counters
    trace_buffer.head = 0;
    trace_buffer.tail = 0;
    trace_buffer.event_count = 0;
    trace_buffer.dropped_events = 0;
    trace_buffer.buffer_full = false;
    
    // Clear all events
    memset(trace_buffer.events, 0, trace_buffer.buffer_size * sizeof(pico_rtos_trace_event_t));
    
    pico_rtos_exit_critical();
}

void pico_rtos_trace_set_overflow_behavior(pico_rtos_trace_overflow_behavior_t behavior) {
    if (trace_initialized) {
        trace_buffer.overflow_behavior = behavior;
    }
}

pico_rtos_trace_overflow_behavior_t pico_rtos_trace_get_overflow_behavior(void) {
    return trace_initialized ? trace_buffer.overflow_behavior : PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP;
}

// =============================================================================
// EVENT RECORDING IMPLEMENTATION
// =============================================================================

bool pico_rtos_trace_record_event(pico_rtos_trace_event_type_t type,
                                 pico_rtos_trace_priority_t priority,
                                 uint32_t task_id,
                                 uint32_t object_id,
                                 uint32_t data1,
                                 uint32_t data2,
                                 const char *event_name) {
    if (!trace_initialized || !trace_buffer.events) {
        return false;
    }
    
    // Check if event should be recorded
    if (!should_record_event(type, priority, task_id)) {
        return false;
    }
    
    pico_rtos_enter_critical();
    
    // Get write position
    uint32_t write_pos = get_next_write_position();
    if (write_pos == INVALID_EVENT_INDEX) {
        trace_buffer.dropped_events++;
        pico_rtos_exit_critical();
        return false;
    }
    
    // Fill event structure
    pico_rtos_trace_event_t *event = &trace_buffer.events[write_pos];
    event->type = type;
    event->priority = priority;
    event->timestamp = get_time_us();
    event->task_id = task_id;
    event->object_id = object_id;
    event->data1 = data1;
    event->data2 = data2;
    
    // Copy event name if provided
    if (event_name) {
        strncpy(event->event_name, event_name, PICO_RTOS_TRACE_EVENT_NAME_MAX_LENGTH - 1);
        event->event_name[PICO_RTOS_TRACE_EVENT_NAME_MAX_LENGTH - 1] = '\0';
    } else {
        event->event_name[0] = '\0';
    }
    
    // Advance write position
    advance_write_position();
    
    pico_rtos_exit_critical();
    
    return true;
}

bool pico_rtos_trace_record_simple(pico_rtos_trace_event_type_t type, uint32_t data) {
    return pico_rtos_trace_record_event(type, PICO_RTOS_TRACE_PRIORITY_NORMAL, 0, 0, data, 0, NULL);
}

bool pico_rtos_trace_record_user_event(const char *event_name, uint32_t data1, uint32_t data2) {
    return pico_rtos_trace_record_event(PICO_RTOS_TRACE_USER_EVENT, PICO_RTOS_TRACE_PRIORITY_NORMAL,
                                       0, 0, data1, data2, event_name);
}

// =============================================================================
// EVENT FILTERING IMPLEMENTATION
// =============================================================================

void pico_rtos_trace_set_type_filter(uint32_t type_mask) {
    if (trace_initialized) {
        trace_buffer.filter_mask = type_mask;
    }
}

uint32_t pico_rtos_trace_get_type_filter(void) {
    return trace_initialized ? trace_buffer.filter_mask : 0;
}

void pico_rtos_trace_set_priority_filter(pico_rtos_trace_priority_t min_priority) {
    if (trace_initialized) {
        trace_buffer.min_priority = min_priority;
    }
}

pico_rtos_trace_priority_t pico_rtos_trace_get_priority_filter(void) {
    return trace_initialized ? trace_buffer.min_priority : PICO_RTOS_TRACE_PRIORITY_LOW;
}

void pico_rtos_trace_set_task_filter(uint32_t task_id) {
    // This would need to be implemented with actual task filtering
    // For now, we'll store it but not use it
    (void)task_id;
}

uint32_t pico_rtos_trace_get_task_filter(void) {
    // This would need to be implemented with actual task filtering
    return 0;
}

// =============================================================================
// EVENT RETRIEVAL IMPLEMENTATION
// =============================================================================

uint32_t pico_rtos_trace_get_event_count(void) {
    if (!trace_initialized) {
        return 0;
    }
    
    return (trace_buffer.event_count < trace_buffer.buffer_size) ? 
           trace_buffer.event_count : trace_buffer.buffer_size;
}

uint32_t pico_rtos_trace_get_events(pico_rtos_trace_event_t *events, 
                                   uint32_t max_events, 
                                   uint32_t start_index) {
    if (!trace_initialized || !events || max_events == 0 || !trace_buffer.events) {
        return 0;
    }
    
    uint32_t available_events = pico_rtos_trace_get_event_count();
    if (start_index >= available_events) {
        return 0;
    }
    
    uint32_t events_to_copy = (max_events < (available_events - start_index)) ? 
                             max_events : (available_events - start_index);
    
    pico_rtos_enter_critical();
    
    uint32_t copied = 0;
    uint32_t read_pos = (trace_buffer.tail + start_index) % trace_buffer.buffer_size;
    
    for (uint32_t i = 0; i < events_to_copy; i++) {
        events[copied] = trace_buffer.events[read_pos];
        copied++;
        read_pos = (read_pos + 1) % trace_buffer.buffer_size;
    }
    
    pico_rtos_exit_critical();
    
    return copied;
}

uint32_t pico_rtos_trace_get_recent_events(pico_rtos_trace_event_t *events, uint32_t max_events) {
    if (!trace_initialized || !events || max_events == 0) {
        return 0;
    }
    
    uint32_t available_events = pico_rtos_trace_get_event_count();
    if (available_events == 0) {
        return 0;
    }
    
    // Start from the most recent events
    uint32_t start_index = (available_events > max_events) ? (available_events - max_events) : 0;
    
    return pico_rtos_trace_get_events(events, max_events, start_index);
}

uint32_t pico_rtos_trace_get_events_by_type(pico_rtos_trace_event_type_t type,
                                           pico_rtos_trace_event_t *events,
                                           uint32_t max_events) {
    if (!trace_initialized || !events || max_events == 0 || !trace_buffer.events) {
        return 0;
    }
    
    uint32_t available_events = pico_rtos_trace_get_event_count();
    if (available_events == 0) {
        return 0;
    }
    
    pico_rtos_enter_critical();
    
    uint32_t copied = 0;
    uint32_t read_pos = trace_buffer.tail;
    
    for (uint32_t i = 0; i < available_events && copied < max_events; i++) {
        if (trace_buffer.events[read_pos].type == type) {
            events[copied] = trace_buffer.events[read_pos];
            copied++;
        }
        read_pos = (read_pos + 1) % trace_buffer.buffer_size;
    }
    
    pico_rtos_exit_critical();
    
    return copied;
}

uint32_t pico_rtos_trace_get_events_by_task(uint32_t task_id,
                                           pico_rtos_trace_event_t *events,
                                           uint32_t max_events) {
    if (!trace_initialized || !events || max_events == 0 || !trace_buffer.events) {
        return 0;
    }
    
    uint32_t available_events = pico_rtos_trace_get_event_count();
    if (available_events == 0) {
        return 0;
    }
    
    pico_rtos_enter_critical();
    
    uint32_t copied = 0;
    uint32_t read_pos = trace_buffer.tail;
    
    for (uint32_t i = 0; i < available_events && copied < max_events; i++) {
        if (trace_buffer.events[read_pos].task_id == task_id) {
            events[copied] = trace_buffer.events[read_pos];
            copied++;
        }
        read_pos = (read_pos + 1) % trace_buffer.buffer_size;
    }
    
    pico_rtos_exit_critical();
    
    return copied;
}

// =============================================================================
// STATISTICS AND ANALYSIS IMPLEMENTATION
// =============================================================================

bool pico_rtos_trace_get_stats(pico_rtos_trace_stats_t *stats) {
    if (!trace_initialized || !stats || !trace_buffer.events) {
        return false;
    }
    
    memset(stats, 0, sizeof(pico_rtos_trace_stats_t));
    
    pico_rtos_enter_critical();
    
    uint32_t available_events = pico_rtos_trace_get_event_count();
    stats->total_events = trace_buffer.event_count;
    stats->dropped_events = trace_buffer.dropped_events;
    stats->buffer_utilization_percent = calculate_buffer_utilization();
    
    if (available_events > 0) {
        // Find first and last event timestamps
        uint32_t read_pos = trace_buffer.tail;
        stats->first_event_time = trace_buffer.events[read_pos].timestamp;
        
        uint32_t last_pos = (trace_buffer.head == 0) ? 
                           (trace_buffer.buffer_size - 1) : (trace_buffer.head - 1);
        stats->last_event_time = trace_buffer.events[last_pos].timestamp;
        
        // Count events by type and priority
        for (uint32_t i = 0; i < available_events; i++) {
            pico_rtos_trace_event_t *event = &trace_buffer.events[read_pos];
            
            if (event->type < PICO_RTOS_TRACE_MAX_EVENT_TYPES) {
                stats->events_by_type[event->type]++;
            }
            
            if (event->priority < 4) {
                stats->events_by_priority[event->priority]++;
            }
            
            read_pos = (read_pos + 1) % trace_buffer.buffer_size;
        }
        
        // Calculate events per second
        uint64_t time_span = stats->last_event_time - stats->first_event_time;
        if (time_span > 0) {
            stats->average_events_per_second = (float)available_events / (time_span / 1000000.0f);
        }
    }
    
    pico_rtos_exit_critical();
    
    return true;
}

void pico_rtos_trace_reset_stats(void) {
    if (!trace_initialized) {
        return;
    }
    
    pico_rtos_enter_critical();
    trace_buffer.dropped_events = 0;
    // Note: We don't reset event_count as it's used for buffer management
    pico_rtos_exit_critical();
}

// =============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// =============================================================================

const char *pico_rtos_trace_event_type_to_string(pico_rtos_trace_event_type_t type) {
    switch (type) {
        case PICO_RTOS_TRACE_TASK_SWITCH: return "TASK_SWITCH";
        case PICO_RTOS_TRACE_TASK_CREATE: return "TASK_CREATE";
        case PICO_RTOS_TRACE_TASK_DELETE: return "TASK_DELETE";
        case PICO_RTOS_TRACE_TASK_SUSPEND: return "TASK_SUSPEND";
        case PICO_RTOS_TRACE_TASK_RESUME: return "TASK_RESUME";
        case PICO_RTOS_TRACE_TASK_DELAY: return "TASK_DELAY";
        case PICO_RTOS_TRACE_TASK_YIELD: return "TASK_YIELD";
        case PICO_RTOS_TRACE_MUTEX_LOCK: return "MUTEX_LOCK";
        case PICO_RTOS_TRACE_MUTEX_UNLOCK: return "MUTEX_UNLOCK";
        case PICO_RTOS_TRACE_MUTEX_LOCK_FAILED: return "MUTEX_LOCK_FAILED";
        case PICO_RTOS_TRACE_MUTEX_CREATE: return "MUTEX_CREATE";
        case PICO_RTOS_TRACE_MUTEX_DELETE: return "MUTEX_DELETE";
        case PICO_RTOS_TRACE_SEMAPHORE_GIVE: return "SEMAPHORE_GIVE";
        case PICO_RTOS_TRACE_SEMAPHORE_TAKE: return "SEMAPHORE_TAKE";
        case PICO_RTOS_TRACE_SEMAPHORE_TAKE_FAILED: return "SEMAPHORE_TAKE_FAILED";
        case PICO_RTOS_TRACE_SEMAPHORE_CREATE: return "SEMAPHORE_CREATE";
        case PICO_RTOS_TRACE_SEMAPHORE_DELETE: return "SEMAPHORE_DELETE";
        case PICO_RTOS_TRACE_QUEUE_SEND: return "QUEUE_SEND";
        case PICO_RTOS_TRACE_QUEUE_RECEIVE: return "QUEUE_RECEIVE";
        case PICO_RTOS_TRACE_QUEUE_SEND_FAILED: return "QUEUE_SEND_FAILED";
        case PICO_RTOS_TRACE_QUEUE_RECEIVE_FAILED: return "QUEUE_RECEIVE_FAILED";
        case PICO_RTOS_TRACE_QUEUE_CREATE: return "QUEUE_CREATE";
        case PICO_RTOS_TRACE_QUEUE_DELETE: return "QUEUE_DELETE";
        case PICO_RTOS_TRACE_EVENT_GROUP_SET: return "EVENT_GROUP_SET";
        case PICO_RTOS_TRACE_EVENT_GROUP_CLEAR: return "EVENT_GROUP_CLEAR";
        case PICO_RTOS_TRACE_EVENT_GROUP_WAIT: return "EVENT_GROUP_WAIT";
        case PICO_RTOS_TRACE_EVENT_GROUP_WAIT_FAILED: return "EVENT_GROUP_WAIT_FAILED";
        case PICO_RTOS_TRACE_EVENT_GROUP_CREATE: return "EVENT_GROUP_CREATE";
        case PICO_RTOS_TRACE_EVENT_GROUP_DELETE: return "EVENT_GROUP_DELETE";
        case PICO_RTOS_TRACE_STREAM_BUFFER_SEND: return "STREAM_BUFFER_SEND";
        case PICO_RTOS_TRACE_STREAM_BUFFER_RECEIVE: return "STREAM_BUFFER_RECEIVE";
        case PICO_RTOS_TRACE_STREAM_BUFFER_SEND_FAILED: return "STREAM_BUFFER_SEND_FAILED";
        case PICO_RTOS_TRACE_STREAM_BUFFER_RECEIVE_FAILED: return "STREAM_BUFFER_RECEIVE_FAILED";
        case PICO_RTOS_TRACE_STREAM_BUFFER_CREATE: return "STREAM_BUFFER_CREATE";
        case PICO_RTOS_TRACE_STREAM_BUFFER_DELETE: return "STREAM_BUFFER_DELETE";
        case PICO_RTOS_TRACE_TIMER_START: return "TIMER_START";
        case PICO_RTOS_TRACE_TIMER_STOP: return "TIMER_STOP";
        case PICO_RTOS_TRACE_TIMER_EXPIRE: return "TIMER_EXPIRE";
        case PICO_RTOS_TRACE_TIMER_CREATE: return "TIMER_CREATE";
        case PICO_RTOS_TRACE_TIMER_DELETE: return "TIMER_DELETE";
        case PICO_RTOS_TRACE_INTERRUPT_ENTER: return "INTERRUPT_ENTER";
        case PICO_RTOS_TRACE_INTERRUPT_EXIT: return "INTERRUPT_EXIT";
        case PICO_RTOS_TRACE_MEMORY_ALLOC: return "MEMORY_ALLOC";
        case PICO_RTOS_TRACE_MEMORY_FREE: return "MEMORY_FREE";
        case PICO_RTOS_TRACE_MEMORY_POOL_ALLOC: return "MEMORY_POOL_ALLOC";
        case PICO_RTOS_TRACE_MEMORY_POOL_FREE: return "MEMORY_POOL_FREE";
        case PICO_RTOS_TRACE_SYSTEM_TICK: return "SYSTEM_TICK";
        case PICO_RTOS_TRACE_SYSTEM_INIT: return "SYSTEM_INIT";
        case PICO_RTOS_TRACE_SYSTEM_START: return "SYSTEM_START";
        case PICO_RTOS_TRACE_SYSTEM_ERROR: return "SYSTEM_ERROR";
        case PICO_RTOS_TRACE_USER_EVENT: return "USER_EVENT";
        default: return "UNKNOWN";
    }
}

const char *pico_rtos_trace_priority_to_string(pico_rtos_trace_priority_t priority) {
    switch (priority) {
        case PICO_RTOS_TRACE_PRIORITY_LOW: return "LOW";
        case PICO_RTOS_TRACE_PRIORITY_NORMAL: return "NORMAL";
        case PICO_RTOS_TRACE_PRIORITY_HIGH: return "HIGH";
        case PICO_RTOS_TRACE_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

uint32_t pico_rtos_trace_format_event(const pico_rtos_trace_event_t *event,
                                     char *buffer, uint32_t buffer_size) {
    if (!event || !buffer || buffer_size == 0) {
        return 0;
    }
    
    const char *event_name = (event->event_name[0] != '\0') ? event->event_name : "N/A";
    
    return snprintf(buffer, buffer_size,
        "[%llu] %s (%s) Task:%u Obj:%u Data:0x%08X,0x%08X Name:%s",
        event->timestamp,
        pico_rtos_trace_event_type_to_string(event->type),
        pico_rtos_trace_priority_to_string(event->priority),
        event->task_id,
        event->object_id,
        event->data1,
        event->data2,
        event_name
    );
}

void pico_rtos_trace_print_event(const pico_rtos_trace_event_t *event) {
    if (!event) {
        return;
    }
    
    char buffer[256];
    pico_rtos_trace_format_event(event, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void pico_rtos_trace_print_stats(const pico_rtos_trace_stats_t *stats) {
    if (!stats) {
        return;
    }
    
    printf("Trace Statistics:\n");
    printf("  Total Events: %u\n", stats->total_events);
    printf("  Dropped Events: %u\n", stats->dropped_events);
    printf("  Buffer Utilization: %u%%\n", stats->buffer_utilization_percent);
    printf("  Average Events/sec: %.2f\n", stats->average_events_per_second);
    printf("  Time Span: %llu us\n", stats->last_event_time - stats->first_event_time);
    
    printf("  Events by Priority:\n");
    printf("    Low: %u, Normal: %u, High: %u, Critical: %u\n",
           stats->events_by_priority[0], stats->events_by_priority[1],
           stats->events_by_priority[2], stats->events_by_priority[3]);
}

void pico_rtos_trace_print_all_events(void) {
    if (!trace_initialized) {
        printf("Trace system not initialized\n");
        return;
    }
    
    uint32_t event_count = pico_rtos_trace_get_event_count();
    if (event_count == 0) {
        printf("No trace events available\n");
        return;
    }
    
    printf("Trace Events (%u total):\n", event_count);
    printf("========================\n");
    
    pico_rtos_trace_event_t events[32]; // Process in chunks
    uint32_t start_index = 0;
    
    while (start_index < event_count) {
        uint32_t chunk_size = (event_count - start_index > 32) ? 32 : (event_count - start_index);
        uint32_t retrieved = pico_rtos_trace_get_events(events, chunk_size, start_index);
        
        for (uint32_t i = 0; i < retrieved; i++) {
            printf("%u: ", start_index + i + 1);
            pico_rtos_trace_print_event(&events[i]);
        }
        
        start_index += retrieved;
        if (retrieved == 0) break; // Safety check
    }
}

void pico_rtos_trace_dump_buffer(void) {
    if (!trace_initialized) {
        printf("Trace system not initialized\n");
        return;
    }
    
    printf("Trace Buffer Dump\n");
    printf("=================\n");
    
    // Print buffer configuration
    printf("Buffer Configuration:\n");
    printf("  Size: %u events\n", trace_buffer.buffer_size);
    printf("  Overflow Behavior: %s\n", 
           (trace_buffer.overflow_behavior == PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP) ? "WRAP" : "STOP");
    printf("  Tracing Enabled: %s\n", trace_buffer.tracing_enabled ? "YES" : "NO");
    printf("  Filter Mask: 0x%08X\n", trace_buffer.filter_mask);
    printf("  Min Priority: %s\n", pico_rtos_trace_priority_to_string(trace_buffer.min_priority));
    printf("\n");
    
    // Print statistics
    pico_rtos_trace_stats_t stats;
    if (pico_rtos_trace_get_stats(&stats)) {
        pico_rtos_trace_print_stats(&stats);
        printf("\n");
    }
    
    // Print all events
    pico_rtos_trace_print_all_events();
}

#endif // PICO_RTOS_ENABLE_SYSTEM_TRACING