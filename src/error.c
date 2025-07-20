/**
 * @file error.c
 * @brief Enhanced Error Reporting System Implementation
 * 
 * This module implements comprehensive error reporting with detailed error codes,
 * context information, and optional error history tracking for debugging.
 */

#include "pico_rtos/error.h"
#include "pico_rtos/task.h"
#include "pico_rtos.h"
#include <string.h>

// =============================================================================
// PRIVATE DATA STRUCTURES
// =============================================================================

/**
 * @brief Error system state
 */
static struct {
    bool initialized;
    pico_rtos_error_info_t last_error;
    pico_rtos_error_callback_t callback;
    pico_rtos_error_stats_t stats;
    
#if PICO_RTOS_ENABLE_ERROR_HISTORY
    pico_rtos_error_history_t history;
    bool history_initialized;
#endif
} error_system = {0};

// =============================================================================
// ERROR DESCRIPTION TABLE
// =============================================================================

/**
 * @brief Error code to description mapping
 */
static const struct {
    pico_rtos_error_t code;
    const char *description;
} error_descriptions[] = {
    // No error
    {PICO_RTOS_ERROR_NONE, "No error"},
    
    // Task errors (100-199)
    {PICO_RTOS_ERROR_TASK_INVALID_PRIORITY, "Invalid task priority specified"},
    {PICO_RTOS_ERROR_TASK_STACK_OVERFLOW, "Task stack overflow detected"},
    {PICO_RTOS_ERROR_TASK_INVALID_STATE, "Task is in invalid state for operation"},
    {PICO_RTOS_ERROR_TASK_INVALID_PARAMETER, "Invalid parameter passed to task function"},
    {PICO_RTOS_ERROR_TASK_STACK_TOO_SMALL, "Task stack size is too small"},
    {PICO_RTOS_ERROR_TASK_FUNCTION_NULL, "Task function pointer is NULL"},
    {PICO_RTOS_ERROR_TASK_NOT_FOUND, "Specified task not found"},
    {PICO_RTOS_ERROR_TASK_ALREADY_TERMINATED, "Task is already terminated"},
    {PICO_RTOS_ERROR_TASK_CANNOT_SUSPEND_SELF, "Task cannot suspend itself"},
    {PICO_RTOS_ERROR_TASK_MAX_TASKS_EXCEEDED, "Maximum number of tasks exceeded"},
    
    // Memory errors (200-299)
    {PICO_RTOS_ERROR_OUT_OF_MEMORY, "Insufficient memory available"},
    {PICO_RTOS_ERROR_INVALID_POINTER, "Invalid or NULL pointer provided"},
    {PICO_RTOS_ERROR_MEMORY_CORRUPTION, "Memory corruption detected"},
    {PICO_RTOS_ERROR_MEMORY_ALIGNMENT, "Memory alignment requirements not met"},
    {PICO_RTOS_ERROR_MEMORY_DOUBLE_FREE, "Attempt to free already freed memory"},
    {PICO_RTOS_ERROR_MEMORY_LEAK_DETECTED, "Memory leak detected"},
    {PICO_RTOS_ERROR_MEMORY_FRAGMENTATION, "Memory fragmentation preventing allocation"},
    {PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED, "Memory pool exhausted"},
    
    // Synchronization errors (300-399)
    {PICO_RTOS_ERROR_MUTEX_TIMEOUT, "Mutex lock operation timed out"},
    {PICO_RTOS_ERROR_MUTEX_NOT_OWNED, "Attempt to unlock mutex not owned by current task"},
    {PICO_RTOS_ERROR_MUTEX_RECURSIVE_LOCK, "Recursive mutex lock not supported"},
    {PICO_RTOS_ERROR_SEMAPHORE_TIMEOUT, "Semaphore take operation timed out"},
    {PICO_RTOS_ERROR_SEMAPHORE_OVERFLOW, "Semaphore count would exceed maximum"},
    {PICO_RTOS_ERROR_SEMAPHORE_INVALID_COUNT, "Invalid semaphore count specified"},
    {PICO_RTOS_ERROR_QUEUE_FULL, "Queue is full, cannot send item"},
    {PICO_RTOS_ERROR_QUEUE_EMPTY, "Queue is empty, cannot receive item"},
    {PICO_RTOS_ERROR_QUEUE_TIMEOUT, "Queue operation timed out"},
    {PICO_RTOS_ERROR_QUEUE_INVALID_SIZE, "Invalid queue size specified"},
    {PICO_RTOS_ERROR_QUEUE_INVALID_ITEM_SIZE, "Invalid queue item size specified"},
    {PICO_RTOS_ERROR_TIMER_INVALID_PERIOD, "Invalid timer period specified"},
    {PICO_RTOS_ERROR_TIMER_NOT_RUNNING, "Timer is not running"},
    {PICO_RTOS_ERROR_TIMER_ALREADY_RUNNING, "Timer is already running"},
    
    // System errors (400-499)
    {PICO_RTOS_ERROR_SYSTEM_NOT_INITIALIZED, "RTOS system not initialized"},
    {PICO_RTOS_ERROR_SYSTEM_ALREADY_INITIALIZED, "RTOS system already initialized"},
    {PICO_RTOS_ERROR_INVALID_CONFIGURATION, "Invalid system configuration"},
    {PICO_RTOS_ERROR_SCHEDULER_NOT_RUNNING, "Scheduler is not running"},
    {PICO_RTOS_ERROR_SCHEDULER_ALREADY_RUNNING, "Scheduler is already running"},
    {PICO_RTOS_ERROR_CRITICAL_SECTION_VIOLATION, "Critical section violation detected"},
    {PICO_RTOS_ERROR_INTERRUPT_CONTEXT_VIOLATION, "Invalid operation in interrupt context"},
    {PICO_RTOS_ERROR_SYSTEM_OVERLOAD, "System overload detected"},
    {PICO_RTOS_ERROR_WATCHDOG_TIMEOUT, "Watchdog timeout occurred"},
    
    // Hardware errors (500-599)
    {PICO_RTOS_ERROR_HARDWARE_FAULT, "Hardware fault detected"},
    {PICO_RTOS_ERROR_HARDWARE_TIMER_FAULT, "Hardware timer fault"},
    {PICO_RTOS_ERROR_HARDWARE_INTERRUPT_FAULT, "Hardware interrupt fault"},
    {PICO_RTOS_ERROR_HARDWARE_CLOCK_FAULT, "Hardware clock fault"},
    {PICO_RTOS_ERROR_HARDWARE_MEMORY_FAULT, "Hardware memory fault"},
    {PICO_RTOS_ERROR_HARDWARE_BUS_FAULT, "Hardware bus fault"},
    {PICO_RTOS_ERROR_HARDWARE_USAGE_FAULT, "Hardware usage fault"},
    
    // Configuration errors (600-699)
    {PICO_RTOS_ERROR_CONFIG_INVALID_TICK_RATE, "Invalid system tick rate configuration"},
    {PICO_RTOS_ERROR_CONFIG_INVALID_STACK_SIZE, "Invalid stack size configuration"},
    {PICO_RTOS_ERROR_CONFIG_INVALID_PRIORITY, "Invalid priority configuration"},
    {PICO_RTOS_ERROR_CONFIG_FEATURE_DISABLED, "Required feature is disabled"},
    {PICO_RTOS_ERROR_CONFIG_DEPENDENCY_MISSING, "Configuration dependency missing"}
};

#define ERROR_DESCRIPTION_COUNT (sizeof(error_descriptions) / sizeof(error_descriptions[0]))

// =============================================================================
// PRIVATE HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current task ID for error reporting
 * 
 * @return Current task ID, or 0 if in interrupt context or no current task
 */
static uint32_t get_current_task_id(void) {
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    return current_task ? (uint32_t)current_task : 0;
}

/**
 * @brief Update error statistics
 * 
 * @param code Error code to update statistics for
 */
static void update_error_stats(pico_rtos_error_t code) {
    error_system.stats.total_errors++;
    error_system.stats.most_recent_error = code;
    error_system.stats.most_recent_timestamp = pico_rtos_get_tick_count();
    
    // Update category-specific counters
    if (code >= 100 && code < 200) {
        error_system.stats.task_errors++;
    } else if (code >= 200 && code < 300) {
        error_system.stats.memory_errors++;
    } else if (code >= 300 && code < 400) {
        error_system.stats.sync_errors++;
    } else if (code >= 400 && code < 500) {
        error_system.stats.system_errors++;
    } else if (code >= 500 && code < 600) {
        error_system.stats.hardware_errors++;
    } else if (code >= 600 && code < 700) {
        error_system.stats.config_errors++;
    }
}

#if PICO_RTOS_ENABLE_ERROR_HISTORY
/**
 * @brief Add error to history buffer
 * 
 * @param error_info Pointer to error information to add
 */
static void add_to_error_history(const pico_rtos_error_info_t *error_info) {
    if (!error_system.history_initialized) {
        return;
    }
    
    pico_rtos_error_history_t *history = &error_system.history;
    
    // Get next entry in circular buffer
    pico_rtos_error_entry_t *entry;
    if (history->count < history->max_count) {
        // Buffer not full yet, use next available entry
        entry = &history->entries[history->count];
        history->count++;
    } else {
        // Buffer is full, reuse oldest entry
        entry = history->head;
        history->head = history->head->next;
    }
    
    // Copy error information
    entry->info = *error_info;
    
    // Update linked list
    if (history->tail) {
        history->tail->next = entry;
    }
    entry->next = NULL;
    history->tail = entry;
    
    // Set head if this is the first entry
    if (!history->head) {
        history->head = entry;
    }
}

/**
 * @brief Initialize error history circular buffer
 */
static void init_error_history(void) {
    pico_rtos_error_history_t *history = &error_system.history;
    
    history->head = NULL;
    history->tail = NULL;
    history->count = 0;
    history->max_count = PICO_RTOS_ERROR_HISTORY_SIZE;
    
    // Initialize circular linked list
    for (size_t i = 0; i < PICO_RTOS_ERROR_HISTORY_SIZE - 1; i++) {
        history->entries[i].next = &history->entries[i + 1];
    }
    history->entries[PICO_RTOS_ERROR_HISTORY_SIZE - 1].next = &history->entries[0];
    
    error_system.history_initialized = true;
}
#endif // PICO_RTOS_ENABLE_ERROR_HISTORY

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_error_init(void) {
    if (error_system.initialized) {
        return true; // Already initialized
    }
    
    // Initialize error system state
    memset(&error_system, 0, sizeof(error_system));
    
    // Set last error to "no error"
    error_system.last_error.code = PICO_RTOS_ERROR_NONE;
    error_system.last_error.description = pico_rtos_get_error_description(PICO_RTOS_ERROR_NONE);
    
#if PICO_RTOS_ENABLE_ERROR_HISTORY
    init_error_history();
#endif
    
    error_system.initialized = true;
    return true;
}

void pico_rtos_report_error_detailed(pico_rtos_error_t code, 
                                   const char *file, 
                                   int line, 
                                   const char *function,
                                   uint32_t context_data) {
    if (!error_system.initialized) {
        // Try to initialize if not already done
        if (!pico_rtos_error_init()) {
            return; // Cannot report error if initialization fails
        }
    }
    
    // Skip if no error
    if (code == PICO_RTOS_ERROR_NONE) {
        return;
    }
    
    // Create error information structure
    pico_rtos_error_info_t error_info = {
        .code = code,
        .timestamp = pico_rtos_get_tick_count(),
        .task_id = get_current_task_id(),
        .file = file,
        .line = line,
        .function = function,
        .description = pico_rtos_get_error_description(code),
        .context_data = context_data
    };
    
    // Update last error
    error_system.last_error = error_info;
    
    // Update statistics
    update_error_stats(code);
    
#if PICO_RTOS_ENABLE_ERROR_HISTORY
    // Add to error history
    add_to_error_history(&error_info);
#endif
    
    // Call user callback if registered
    if (error_system.callback) {
        error_system.callback(&error_info);
    }
}

const char *pico_rtos_get_error_description(pico_rtos_error_t code) {
    // Search for error code in description table
    for (size_t i = 0; i < ERROR_DESCRIPTION_COUNT; i++) {
        if (error_descriptions[i].code == code) {
            return error_descriptions[i].description;
        }
    }
    
    // Return generic description for unknown error codes
    return "Unknown error code";
}

bool pico_rtos_get_last_error(pico_rtos_error_info_t *error_info) {
    if (!error_info || !error_system.initialized) {
        return false;
    }
    
    if (error_system.last_error.code == PICO_RTOS_ERROR_NONE) {
        return false; // No error has occurred
    }
    
    *error_info = error_system.last_error;
    return true;
}

void pico_rtos_clear_last_error(void) {
    if (!error_system.initialized) {
        return;
    }
    
    error_system.last_error.code = PICO_RTOS_ERROR_NONE;
    error_system.last_error.description = pico_rtos_get_error_description(PICO_RTOS_ERROR_NONE);
    error_system.last_error.timestamp = 0;
    error_system.last_error.task_id = 0;
    error_system.last_error.file = NULL;
    error_system.last_error.line = 0;
    error_system.last_error.function = NULL;
    error_system.last_error.context_data = 0;
}

// =============================================================================
// ERROR HISTORY API IMPLEMENTATION
// =============================================================================

#if PICO_RTOS_ENABLE_ERROR_HISTORY

bool pico_rtos_get_error_history(pico_rtos_error_info_t *errors, 
                                size_t max_count, 
                                size_t *actual_count) {
    if (!errors || !actual_count || !error_system.initialized || !error_system.history_initialized) {
        return false;
    }
    
    pico_rtos_error_history_t *history = &error_system.history;
    *actual_count = 0;
    
    if (history->count == 0) {
        return true; // No errors in history
    }
    
    // Copy errors from history buffer (oldest first)
    pico_rtos_error_entry_t *current = history->head;
    size_t copied = 0;
    
    while (current && copied < max_count && copied < history->count) {
        errors[copied] = current->info;
        copied++;
        current = current->next;
        
        // Break if we've wrapped around to the beginning
        if (current == history->head && copied > 0) {
            break;
        }
    }
    
    *actual_count = copied;
    return true;
}

size_t pico_rtos_get_error_count(void) {
    if (!error_system.initialized || !error_system.history_initialized) {
        return 0;
    }
    
    return error_system.history.count;
}

void pico_rtos_clear_error_history(void) {
    if (!error_system.initialized || !error_system.history_initialized) {
        return;
    }
    
    pico_rtos_error_history_t *history = &error_system.history;
    history->head = NULL;
    history->tail = NULL;
    history->count = 0;
}

bool pico_rtos_is_error_history_full(void) {
    if (!error_system.initialized || !error_system.history_initialized) {
        return false;
    }
    
    return error_system.history.count >= error_system.history.max_count;
}

#endif // PICO_RTOS_ENABLE_ERROR_HISTORY

// =============================================================================
// ERROR STATISTICS API IMPLEMENTATION
// =============================================================================

void pico_rtos_get_error_stats(pico_rtos_error_stats_t *stats) {
    if (!stats || !error_system.initialized) {
        return;
    }
    
    *stats = error_system.stats;
}

void pico_rtos_reset_error_stats(void) {
    if (!error_system.initialized) {
        return;
    }
    
    memset(&error_system.stats, 0, sizeof(error_system.stats));
}

// =============================================================================
// ERROR CALLBACK API IMPLEMENTATION
// =============================================================================

void pico_rtos_set_error_callback(pico_rtos_error_callback_t callback) {
    if (!error_system.initialized) {
        return;
    }
    
    error_system.callback = callback;
}

pico_rtos_error_callback_t pico_rtos_get_error_callback(void) {
    if (!error_system.initialized) {
        return NULL;
    }
    
    return error_system.callback;
}