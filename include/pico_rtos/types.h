#ifndef PICO_RTOS_TYPES_H
#define PICO_RTOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Common constants
#define PICO_RTOS_TASK_NAME_MAX_LENGTH 32
#define PICO_RTOS_WAIT_FOREVER UINT32_MAX
#define PICO_RTOS_NO_WAIT 0

// Task states
typedef enum {
    PICO_RTOS_TASK_STATE_READY,
    PICO_RTOS_TASK_STATE_RUNNING,
    PICO_RTOS_TASK_STATE_BLOCKED,    // Blocked on a resource or delay
    PICO_RTOS_TASK_STATE_SUSPENDED,  // Explicitly suspended
    PICO_RTOS_TASK_STATE_TERMINATED  // Task has terminated
} pico_rtos_task_state_t;

// Block reasons
typedef enum {
    PICO_RTOS_BLOCK_REASON_NONE,
    PICO_RTOS_BLOCK_REASON_DELAY,
    PICO_RTOS_BLOCK_REASON_QUEUE_FULL,
    PICO_RTOS_BLOCK_REASON_QUEUE_EMPTY,
    PICO_RTOS_BLOCK_REASON_SEMAPHORE,
    PICO_RTOS_BLOCK_REASON_MUTEX,
    PICO_RTOS_BLOCK_REASON_EVENT_GROUP,
    PICO_RTOS_BLOCK_REASON_STREAM_BUFFER
} pico_rtos_block_reason_t;

// Function pointer types
typedef void (*pico_rtos_task_function_t)(void *param);

// SMP-specific types (v0.3.1)
// Note: Core affinity type is always defined for API consistency,
// but functionality is only available when PICO_RTOS_ENABLE_MULTI_CORE is enabled
/**
 * @brief Core affinity options for tasks
 */
typedef enum {
    PICO_RTOS_CORE_AFFINITY_NONE = 0,     ///< No core preference (can run anywhere)
    PICO_RTOS_CORE_AFFINITY_CORE0 = 1,    ///< Bind to core 0 only
    PICO_RTOS_CORE_AFFINITY_CORE1 = 2,    ///< Bind to core 1 only
    PICO_RTOS_CORE_AFFINITY_ANY = 3       ///< Can run on any available core
} pico_rtos_core_affinity_t;

#endif // PICO_RTOS_TYPES_H