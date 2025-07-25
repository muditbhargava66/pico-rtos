#ifndef PICO_RTOS_TRACE_H
#define PICO_RTOS_TRACE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file trace.h
 * @brief Pico-RTOS System Event Tracing System
 * 
 * This module provides configurable system event tracing with circular
 * buffer management and configurable overflow behavior for debugging
 * and system analysis.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

/**
 * @brief Enable system event tracing
 * 
 * When disabled, all tracing functions compile to empty stubs with zero overhead.
 */
#ifndef PICO_RTOS_ENABLE_SYSTEM_TRACING
#define PICO_RTOS_ENABLE_SYSTEM_TRACING 0
#endif

/**
 * @brief Default trace buffer size in events
 */
#ifndef PICO_RTOS_TRACE_BUFFER_SIZE
#define PICO_RTOS_TRACE_BUFFER_SIZE 256
#endif

/**
 * @brief Enable trace buffer wrap-around behavior
 * 
 * When enabled, new events overwrite old ones when buffer is full.
 * When disabled, new events are dropped when buffer is full.
 */
#ifndef PICO_RTOS_TRACE_OVERFLOW_WRAP
#define PICO_RTOS_TRACE_OVERFLOW_WRAP 1
#endif

/**
 * @brief Maximum length of trace event names
 */
#ifndef PICO_RTOS_TRACE_EVENT_NAME_MAX_LENGTH
#define PICO_RTOS_TRACE_EVENT_NAME_MAX_LENGTH 32
#endif

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief System event types for tracing
 */
typedef enum {
    PICO_RTOS_TRACE_TASK_SWITCH,          ///< Task context switch
    PICO_RTOS_TRACE_TASK_CREATE,          ///< Task creation
    PICO_RTOS_TRACE_TASK_DELETE,          ///< Task deletion
    PICO_RTOS_TRACE_TASK_SUSPEND,         ///< Task suspension
    PICO_RTOS_TRACE_TASK_RESUME,          ///< Task resumption
    PICO_RTOS_TRACE_TASK_DELAY,           ///< Task delay
    PICO_RTOS_TRACE_TASK_YIELD,           ///< Task yield
    
    PICO_RTOS_TRACE_MUTEX_LOCK,           ///< Mutex lock operation
    PICO_RTOS_TRACE_MUTEX_UNLOCK,         ///< Mutex unlock operation
    PICO_RTOS_TRACE_MUTEX_LOCK_FAILED,    ///< Mutex lock failed
    PICO_RTOS_TRACE_MUTEX_CREATE,         ///< Mutex creation
    PICO_RTOS_TRACE_MUTEX_DELETE,         ///< Mutex deletion
    
    PICO_RTOS_TRACE_SEMAPHORE_GIVE,       ///< Semaphore give operation
    PICO_RTOS_TRACE_SEMAPHORE_TAKE,       ///< Semaphore take operation
    PICO_RTOS_TRACE_SEMAPHORE_TAKE_FAILED,///< Semaphore take failed
    PICO_RTOS_TRACE_SEMAPHORE_CREATE,     ///< Semaphore creation
    PICO_RTOS_TRACE_SEMAPHORE_DELETE,     ///< Semaphore deletion
    
    PICO_RTOS_TRACE_QUEUE_SEND,           ///< Queue send operation
    PICO_RTOS_TRACE_QUEUE_RECEIVE,        ///< Queue receive operation
    PICO_RTOS_TRACE_QUEUE_SEND_FAILED,    ///< Queue send failed
    PICO_RTOS_TRACE_QUEUE_RECEIVE_FAILED, ///< Queue receive failed
    PICO_RTOS_TRACE_QUEUE_CREATE,         ///< Queue creation
    PICO_RTOS_TRACE_QUEUE_DELETE,         ///< Queue deletion
    
    PICO_RTOS_TRACE_EVENT_GROUP_SET,      ///< Event group set bits
    PICO_RTOS_TRACE_EVENT_GROUP_CLEAR,    ///< Event group clear bits
    PICO_RTOS_TRACE_EVENT_GROUP_WAIT,     ///< Event group wait
    PICO_RTOS_TRACE_EVENT_GROUP_WAIT_FAILED, ///< Event group wait failed
    PICO_RTOS_TRACE_EVENT_GROUP_CREATE,   ///< Event group creation
    PICO_RTOS_TRACE_EVENT_GROUP_DELETE,   ///< Event group deletion
    
    PICO_RTOS_TRACE_STREAM_BUFFER_SEND,   ///< Stream buffer send
    PICO_RTOS_TRACE_STREAM_BUFFER_RECEIVE,///< Stream buffer receive
    PICO_RTOS_TRACE_STREAM_BUFFER_SEND_FAILED, ///< Stream buffer send failed
    PICO_RTOS_TRACE_STREAM_BUFFER_RECEIVE_FAILED, ///< Stream buffer receive failed
    PICO_RTOS_TRACE_STREAM_BUFFER_CREATE, ///< Stream buffer creation
    PICO_RTOS_TRACE_STREAM_BUFFER_DELETE, ///< Stream buffer deletion
    
    PICO_RTOS_TRACE_TIMER_START,          ///< Timer start
    PICO_RTOS_TRACE_TIMER_STOP,           ///< Timer stop
    PICO_RTOS_TRACE_TIMER_EXPIRE,         ///< Timer expiration
    PICO_RTOS_TRACE_TIMER_CREATE,         ///< Timer creation
    PICO_RTOS_TRACE_TIMER_DELETE,         ///< Timer deletion
    
    PICO_RTOS_TRACE_INTERRUPT_ENTER,      ///< Interrupt entry
    PICO_RTOS_TRACE_INTERRUPT_EXIT,       ///< Interrupt exit
    
    PICO_RTOS_TRACE_MEMORY_ALLOC,         ///< Memory allocation
    PICO_RTOS_TRACE_MEMORY_FREE,          ///< Memory deallocation
    PICO_RTOS_TRACE_MEMORY_POOL_ALLOC,    ///< Memory pool allocation
    PICO_RTOS_TRACE_MEMORY_POOL_FREE,     ///< Memory pool deallocation
    
    PICO_RTOS_TRACE_SYSTEM_TICK,          ///< System tick
    PICO_RTOS_TRACE_SYSTEM_INIT,          ///< System initialization
    PICO_RTOS_TRACE_SYSTEM_START,         ///< System start
    PICO_RTOS_TRACE_SYSTEM_ERROR,         ///< System error
    
    PICO_RTOS_TRACE_USER_EVENT,           ///< User-defined event
    PICO_RTOS_TRACE_MAX_EVENT_TYPES       ///< Maximum event types (for validation)
} pico_rtos_trace_event_type_t;

/**
 * @brief Trace event priority levels
 */
typedef enum {
    PICO_RTOS_TRACE_PRIORITY_LOW,         ///< Low priority event
    PICO_RTOS_TRACE_PRIORITY_NORMAL,      ///< Normal priority event
    PICO_RTOS_TRACE_PRIORITY_HIGH,        ///< High priority event
    PICO_RTOS_TRACE_PRIORITY_CRITICAL     ///< Critical priority event
} pico_rtos_trace_priority_t;

/**
 * @brief Individual trace event structure
 */
typedef struct {
    pico_rtos_trace_event_type_t type;     ///< Event type
    pico_rtos_trace_priority_t priority;   ///< Event priority
    uint64_t timestamp;                    ///< Event timestamp in microseconds
    uint32_t task_id;                      ///< Associated task ID (if applicable)
    uint32_t object_id;                    ///< Associated object ID (if applicable)
    uint32_t data1;                        ///< Event-specific data 1
    uint32_t data2;                        ///< Event-specific data 2
    char event_name[PICO_RTOS_TRACE_EVENT_NAME_MAX_LENGTH]; ///< Event name (optional)
} pico_rtos_trace_event_t;

/**
 * @brief Trace buffer overflow behavior
 */
typedef enum {
    PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP,  ///< Wrap around and overwrite old events
    PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_STOP   ///< Stop recording new events when full
} pico_rtos_trace_overflow_behavior_t;

/**
 * @brief Trace buffer configuration
 */
typedef struct {
    pico_rtos_trace_event_t *events;       ///< Event buffer array
    uint32_t buffer_size;                  ///< Buffer size in events
    uint32_t head;                         ///< Write position
    uint32_t tail;                         ///< Read position (for wrap-around)
    uint32_t event_count;                  ///< Total events recorded
    uint32_t dropped_events;               ///< Number of dropped events
    bool buffer_full;                      ///< Buffer full flag
    pico_rtos_trace_overflow_behavior_t overflow_behavior; ///< Overflow behavior
    bool tracing_enabled;                  ///< Tracing enabled flag
    uint32_t filter_mask;                  ///< Event type filter mask
    pico_rtos_trace_priority_t min_priority; ///< Minimum priority to record
} pico_rtos_trace_buffer_t;

/**
 * @brief Trace statistics
 */
typedef struct {
    uint32_t total_events;                 ///< Total events recorded
    uint32_t dropped_events;               ///< Events dropped due to buffer full
    uint32_t events_by_type[PICO_RTOS_TRACE_MAX_EVENT_TYPES]; ///< Events by type
    uint32_t events_by_priority[4];        ///< Events by priority level
    uint64_t first_event_time;             ///< Timestamp of first event
    uint64_t last_event_time;              ///< Timestamp of last event
    uint32_t buffer_utilization_percent;   ///< Buffer utilization percentage
    uint32_t max_events_per_second;        ///< Peak events per second
    float average_events_per_second;       ///< Average events per second
} pico_rtos_trace_stats_t;

/**
 * @brief Event filter configuration
 */
typedef struct {
    uint32_t type_mask;                    ///< Bitmask for event types to record
    pico_rtos_trace_priority_t min_priority; ///< Minimum priority to record
    uint32_t task_filter;                  ///< Task ID filter (0 = all tasks)
    uint32_t object_filter;                ///< Object ID filter (0 = all objects)
    bool enabled;                          ///< Filter enabled flag
} pico_rtos_trace_filter_t;

// =============================================================================
// TRACE BUFFER MANAGEMENT API
// =============================================================================

#if PICO_RTOS_ENABLE_SYSTEM_TRACING

/**
 * @brief Initialize the trace buffer system
 * 
 * Sets up the trace buffer with the specified size and configuration.
 * Must be called before any other tracing functions.
 * 
 * @param buffer_size Size of the trace buffer in events
 * @param overflow_behavior Behavior when buffer is full
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_trace_init(uint32_t buffer_size, pico_rtos_trace_overflow_behavior_t overflow_behavior);

/**
 * @brief Deinitialize the trace buffer system
 * 
 * Cleans up trace buffer resources and disables tracing.
 */
void pico_rtos_trace_deinit(void);

/**
 * @brief Enable or disable tracing
 * 
 * When disabled, trace calls have minimal overhead.
 * 
 * @param enable true to enable tracing, false to disable
 */
void pico_rtos_trace_enable(bool enable);

/**
 * @brief Check if tracing is enabled
 * 
 * @return true if tracing is enabled, false otherwise
 */
bool pico_rtos_trace_is_enabled(void);

/**
 * @brief Clear all events from the trace buffer
 * 
 * Resets the trace buffer to empty state but keeps configuration.
 */
void pico_rtos_trace_clear(void);

/**
 * @brief Set trace buffer overflow behavior
 * 
 * @param behavior Overflow behavior to set
 */
void pico_rtos_trace_set_overflow_behavior(pico_rtos_trace_overflow_behavior_t behavior);

/**
 * @brief Get current trace buffer overflow behavior
 * 
 * @return Current overflow behavior
 */
pico_rtos_trace_overflow_behavior_t pico_rtos_trace_get_overflow_behavior(void);

// =============================================================================
// EVENT RECORDING API
// =============================================================================

/**
 * @brief Record a trace event
 * 
 * Records a trace event with the specified parameters.
 * 
 * @param type Event type
 * @param priority Event priority
 * @param task_id Associated task ID (0 if not applicable)
 * @param object_id Associated object ID (0 if not applicable)
 * @param data1 Event-specific data 1
 * @param data2 Event-specific data 2
 * @param event_name Event name (optional, can be NULL)
 * @return true if event was recorded, false if dropped
 */
bool pico_rtos_trace_record_event(pico_rtos_trace_event_type_t type,
                                 pico_rtos_trace_priority_t priority,
                                 uint32_t task_id,
                                 uint32_t object_id,
                                 uint32_t data1,
                                 uint32_t data2,
                                 const char *event_name);

/**
 * @brief Record a simple trace event
 * 
 * Simplified version for recording events with minimal parameters.
 * 
 * @param type Event type
 * @param data Event-specific data
 * @return true if event was recorded, false if dropped
 */
bool pico_rtos_trace_record_simple(pico_rtos_trace_event_type_t type, uint32_t data);

/**
 * @brief Record a user-defined event
 * 
 * Records a user-defined event with custom data.
 * 
 * @param event_name Name of the user event
 * @param data1 User data 1
 * @param data2 User data 2
 * @return true if event was recorded, false if dropped
 */
bool pico_rtos_trace_record_user_event(const char *event_name, uint32_t data1, uint32_t data2);

// =============================================================================
// EVENT FILTERING API
// =============================================================================

/**
 * @brief Set event type filter
 * 
 * Sets a bitmask to filter which event types are recorded.
 * 
 * @param type_mask Bitmask of event types to record (bit N = event type N)
 */
void pico_rtos_trace_set_type_filter(uint32_t type_mask);

/**
 * @brief Get current event type filter
 * 
 * @return Current event type filter mask
 */
uint32_t pico_rtos_trace_get_type_filter(void);

/**
 * @brief Set minimum priority filter
 * 
 * Sets the minimum priority level for events to be recorded.
 * 
 * @param min_priority Minimum priority level
 */
void pico_rtos_trace_set_priority_filter(pico_rtos_trace_priority_t min_priority);

/**
 * @brief Get current minimum priority filter
 * 
 * @return Current minimum priority level
 */
pico_rtos_trace_priority_t pico_rtos_trace_get_priority_filter(void);

/**
 * @brief Set task filter
 * 
 * Sets a task ID filter to only record events from specific task.
 * 
 * @param task_id Task ID to filter (0 = record all tasks)
 */
void pico_rtos_trace_set_task_filter(uint32_t task_id);

/**
 * @brief Get current task filter
 * 
 * @return Current task ID filter (0 = all tasks)
 */
uint32_t pico_rtos_trace_get_task_filter(void);

// =============================================================================
// EVENT RETRIEVAL API
// =============================================================================

/**
 * @brief Get the number of events in the buffer
 * 
 * @return Number of events currently in the buffer
 */
uint32_t pico_rtos_trace_get_event_count(void);

/**
 * @brief Get events from the trace buffer
 * 
 * Retrieves events from the trace buffer in chronological order.
 * 
 * @param events Array to store retrieved events
 * @param max_events Maximum number of events to retrieve
 * @param start_index Starting index in the buffer (0 = oldest)
 * @return Number of events actually retrieved
 */
uint32_t pico_rtos_trace_get_events(pico_rtos_trace_event_t *events, 
                                   uint32_t max_events, 
                                   uint32_t start_index);

/**
 * @brief Get the most recent events
 * 
 * Retrieves the most recent events from the trace buffer.
 * 
 * @param events Array to store retrieved events
 * @param max_events Maximum number of events to retrieve
 * @return Number of events actually retrieved
 */
uint32_t pico_rtos_trace_get_recent_events(pico_rtos_trace_event_t *events, uint32_t max_events);

/**
 * @brief Get events by type
 * 
 * Retrieves all events of a specific type from the trace buffer.
 * 
 * @param type Event type to filter
 * @param events Array to store retrieved events
 * @param max_events Maximum number of events to retrieve
 * @return Number of events actually retrieved
 */
uint32_t pico_rtos_trace_get_events_by_type(pico_rtos_trace_event_type_t type,
                                           pico_rtos_trace_event_t *events,
                                           uint32_t max_events);

/**
 * @brief Get events by task
 * 
 * Retrieves all events associated with a specific task.
 * 
 * @param task_id Task ID to filter
 * @param events Array to store retrieved events
 * @param max_events Maximum number of events to retrieve
 * @return Number of events actually retrieved
 */
uint32_t pico_rtos_trace_get_events_by_task(uint32_t task_id,
                                           pico_rtos_trace_event_t *events,
                                           uint32_t max_events);

// =============================================================================
// STATISTICS AND ANALYSIS API
// =============================================================================

/**
 * @brief Get trace statistics
 * 
 * Provides comprehensive statistics about the trace buffer and events.
 * 
 * @param stats Pointer to structure to fill with statistics
 * @return true if statistics were retrieved successfully, false otherwise
 */
bool pico_rtos_trace_get_stats(pico_rtos_trace_stats_t *stats);

/**
 * @brief Reset trace statistics
 * 
 * Resets all trace statistics counters but keeps events in buffer.
 */
void pico_rtos_trace_reset_stats(void);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Convert event type to string
 * 
 * @param type Event type to convert
 * @return String representation of the event type
 */
const char *pico_rtos_trace_event_type_to_string(pico_rtos_trace_event_type_t type);

/**
 * @brief Convert priority to string
 * 
 * @param priority Priority to convert
 * @return String representation of the priority
 */
const char *pico_rtos_trace_priority_to_string(pico_rtos_trace_priority_t priority);

/**
 * @brief Format trace event as string
 * 
 * Formats a trace event into a human-readable string.
 * 
 * @param event Event to format
 * @param buffer Buffer to store formatted string
 * @param buffer_size Size of the buffer
 * @return Number of characters written (excluding null terminator)
 */
uint32_t pico_rtos_trace_format_event(const pico_rtos_trace_event_t *event,
                                     char *buffer, uint32_t buffer_size);

/**
 * @brief Print trace event to console
 * 
 * Prints formatted trace event to the console.
 * 
 * @param event Event to print
 */
void pico_rtos_trace_print_event(const pico_rtos_trace_event_t *event);

/**
 * @brief Print trace statistics to console
 * 
 * Prints formatted trace statistics to the console.
 * 
 * @param stats Statistics to print
 */
void pico_rtos_trace_print_stats(const pico_rtos_trace_stats_t *stats);

/**
 * @brief Print all events in the buffer
 * 
 * Prints all events in the trace buffer in chronological order.
 */
void pico_rtos_trace_print_all_events(void);

/**
 * @brief Dump trace buffer to console
 * 
 * Dumps the entire trace buffer with statistics and events.
 */
void pico_rtos_trace_dump_buffer(void);

// =============================================================================
// TRACE HOOK MACROS
// =============================================================================

/**
 * @brief Macro to trace task context switches
 */
#define PICO_RTOS_TRACE_TASK_SWITCHED_IN(task_id) \
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, task_id)

/**
 * @brief Macro to trace task creation
 */
#define PICO_RTOS_TRACE_TASK_CREATED(task_id, priority) \
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_TASK_CREATE, PICO_RTOS_TRACE_PRIORITY_NORMAL, \
                                task_id, 0, priority, 0, NULL)

/**
 * @brief Macro to trace mutex operations
 */
#define PICO_RTOS_TRACE_MUTEX_LOCKED(mutex_id, task_id) \
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_MUTEX_LOCK, PICO_RTOS_TRACE_PRIORITY_NORMAL, \
                                task_id, mutex_id, 0, 0, NULL)

/**
 * @brief Macro to trace queue operations
 */
#define PICO_RTOS_TRACE_QUEUE_SENT(queue_id, task_id, items_in_queue) \
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_QUEUE_SEND, PICO_RTOS_TRACE_PRIORITY_NORMAL, \
                                task_id, queue_id, items_in_queue, 0, NULL)

/**
 * @brief Macro to trace user events
 */
#define PICO_RTOS_TRACE_USER(name, data1, data2) \
    pico_rtos_trace_record_user_event(name, data1, data2)

#else // PICO_RTOS_ENABLE_SYSTEM_TRACING

// When tracing is disabled, all functions become empty stubs
#define pico_rtos_trace_init(size, behavior) (true)
#define pico_rtos_trace_deinit() ((void)0)
#define pico_rtos_trace_enable(enable) ((void)0)
#define pico_rtos_trace_is_enabled() (false)
#define pico_rtos_trace_clear() ((void)0)
#define pico_rtos_trace_set_overflow_behavior(behavior) ((void)0)
#define pico_rtos_trace_get_overflow_behavior() (PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP)
#define pico_rtos_trace_record_event(type, priority, task, obj, d1, d2, name) (false)
#define pico_rtos_trace_record_simple(type, data) (false)
#define pico_rtos_trace_record_user_event(name, d1, d2) (false)
#define pico_rtos_trace_set_type_filter(mask) ((void)0)
#define pico_rtos_trace_get_type_filter() (0)
#define pico_rtos_trace_set_priority_filter(priority) ((void)0)
#define pico_rtos_trace_get_priority_filter() (PICO_RTOS_TRACE_PRIORITY_LOW)
#define pico_rtos_trace_set_task_filter(task_id) ((void)0)
#define pico_rtos_trace_get_task_filter() (0)
#define pico_rtos_trace_get_event_count() (0)
#define pico_rtos_trace_get_events(events, max, start) (0)
#define pico_rtos_trace_get_recent_events(events, max) (0)
#define pico_rtos_trace_get_events_by_type(type, events, max) (0)
#define pico_rtos_trace_get_events_by_task(task, events, max) (0)
#define pico_rtos_trace_get_stats(stats) (false)
#define pico_rtos_trace_reset_stats() ((void)0)
#define pico_rtos_trace_event_type_to_string(type) ("DISABLED")
#define pico_rtos_trace_priority_to_string(priority) ("DISABLED")
#define pico_rtos_trace_format_event(event, buffer, size) (0)
#define pico_rtos_trace_print_event(event) ((void)0)
#define pico_rtos_trace_print_stats(stats) ((void)0)
#define pico_rtos_trace_print_all_events() ((void)0)
#define pico_rtos_trace_dump_buffer() ((void)0)

// Trace hook macros become no-ops
#define PICO_RTOS_TRACE_TASK_SWITCHED_IN(task_id) ((void)0)
#define PICO_RTOS_TRACE_TASK_CREATED(task_id, priority) ((void)0)
#define PICO_RTOS_TRACE_MUTEX_LOCKED(mutex_id, task_id) ((void)0)
#define PICO_RTOS_TRACE_QUEUE_SENT(queue_id, task_id, items) ((void)0)
#define PICO_RTOS_TRACE_USER(name, data1, data2) ((void)0)

#endif // PICO_RTOS_ENABLE_SYSTEM_TRACING

#endif // PICO_RTOS_TRACE_H