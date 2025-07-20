/**
 * @file logging.c
 * @brief Pico-RTOS Debug Logging System Implementation
 * 
 * This file implements the configurable debug logging system for Pico-RTOS.
 * The logging system is only compiled when PICO_RTOS_ENABLE_LOGGING is enabled.
 */

#include "pico_rtos/logging.h"

#if PICO_RTOS_ENABLE_LOGGING

#include <stdio.h>
#include <string.h>
#include "pico_rtos/task.h"
#include "pico_rtos/timer.h"
#include "hardware/sync.h"

// =============================================================================
// INTERNAL DATA STRUCTURES
// =============================================================================

/**
 * @brief Logging system state
 */
typedef struct {
    pico_rtos_log_level_t current_level;        ///< Current log level filter
    uint32_t enabled_subsystems;                ///< Bitmask of enabled subsystems
    pico_rtos_log_output_func_t output_func;    ///< Output function pointer
    bool initialized;                           ///< Initialization flag
    spin_lock_t *lock;                          ///< Spinlock for thread safety
} pico_rtos_log_state_t;

// =============================================================================
// STATIC VARIABLES
// =============================================================================

/**
 * @brief Global logging state
 */
static pico_rtos_log_state_t g_log_state = {
    .current_level = PICO_RTOS_DEFAULT_LOG_LEVEL,
    .enabled_subsystems = PICO_RTOS_DEFAULT_LOG_SUBSYSTEMS,
    .output_func = NULL,
    .initialized = false,
    .lock = NULL
};

/**
 * @brief Log level string representations
 */
static const char *g_log_level_strings[] = {
    "NONE",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG"
};

/**
 * @brief Subsystem string representations
 */
static const char *g_subsystem_strings[] = {
    "CORE",      // PICO_RTOS_LOG_SUBSYSTEM_CORE = 0x01
    "TASK",      // PICO_RTOS_LOG_SUBSYSTEM_TASK = 0x02
    "MUTEX",     // PICO_RTOS_LOG_SUBSYSTEM_MUTEX = 0x04
    "QUEUE",     // PICO_RTOS_LOG_SUBSYSTEM_QUEUE = 0x08
    "TIMER",     // PICO_RTOS_LOG_SUBSYSTEM_TIMER = 0x10
    "MEMORY",    // PICO_RTOS_LOG_SUBSYSTEM_MEMORY = 0x20
    "SEMAPHORE"  // PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE = 0x40
};

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get the current task ID
 * 
 * @return Current task ID, or 0 if in ISR context or task system not initialized
 */
static uint32_t get_current_task_id(void) {
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task != NULL) {
        // Use task priority as a simple ID for now
        // In a more sophisticated system, tasks would have unique IDs
        return current_task->priority;
    }
    return 0; // ISR context or no current task
}

/**
 * @brief Get system timestamp in ticks
 * 
 * @return Current system tick count
 */
static uint32_t get_system_timestamp(void) {
    return pico_rtos_get_tick_count();
}

/**
 * @brief Get subsystem string from bitmask
 * 
 * @param subsystem Subsystem bitmask (should have only one bit set)
 * @return String representation of the subsystem
 */
static const char *subsystem_to_string(pico_rtos_log_subsystem_t subsystem) {
    // Find the first set bit and return corresponding string
    for (int i = 0; i < 7; i++) {
        if (subsystem & (1 << i)) {
            return g_subsystem_strings[i];
        }
    }
    return "UNKNOWN";
}

/**
 * @brief Extract filename from full path
 * 
 * @param path Full file path
 * @return Pointer to filename portion
 */
static const char *extract_filename(const char *path) {
    if (path == NULL) {
        return "unknown";
    }
    
    const char *filename = path;
    const char *p = path;
    
    // Find the last '/' or '\' character
    while (*p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }
    
    return filename;
}

// =============================================================================
// PUBLIC FUNCTION IMPLEMENTATIONS
// =============================================================================

void pico_rtos_log_init(pico_rtos_log_output_func_t output_func) {
    // Initialize spinlock for thread safety
    if (g_log_state.lock == NULL) {
        g_log_state.lock = spin_lock_init(spin_lock_claim_unused(true));
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    
    g_log_state.output_func = output_func;
    g_log_state.current_level = PICO_RTOS_DEFAULT_LOG_LEVEL;
    g_log_state.enabled_subsystems = PICO_RTOS_DEFAULT_LOG_SUBSYSTEMS;
    g_log_state.initialized = true;
    
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_set_level(pico_rtos_log_level_t level) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.current_level = level;
    spin_unlock(g_log_state.lock, save);
}

pico_rtos_log_level_t pico_rtos_log_get_level(void) {
    if (!g_log_state.initialized) {
        return PICO_RTOS_LOG_LEVEL_NONE;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    pico_rtos_log_level_t level = g_log_state.current_level;
    spin_unlock(g_log_state.lock, save);
    
    return level;
}

void pico_rtos_log_enable_subsystem(uint32_t subsystem_mask) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.enabled_subsystems |= subsystem_mask;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_disable_subsystem(uint32_t subsystem_mask) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.enabled_subsystems &= ~subsystem_mask;
    spin_unlock(g_log_state.lock, save);
}

bool pico_rtos_log_is_subsystem_enabled(pico_rtos_log_subsystem_t subsystem) {
    if (!g_log_state.initialized) {
        return false;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    bool enabled = (g_log_state.enabled_subsystems & subsystem) != 0;
    spin_unlock(g_log_state.lock, save);
    
    return enabled;
}

void pico_rtos_log_set_output_func(pico_rtos_log_output_func_t output_func) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.output_func = output_func;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log(pico_rtos_log_level_t level, 
                   pico_rtos_log_subsystem_t subsystem,
                   const char *file, 
                   int line, 
                   const char *format, 
                   ...) {
    // Early exit if logging not initialized
    if (!g_log_state.initialized) {
        return;
    }
    
    // Early exit if level is too high
    if (level > g_log_state.current_level) {
        return;
    }
    
    // Early exit if subsystem is not enabled
    if (!(g_log_state.enabled_subsystems & subsystem)) {
        return;
    }
    
    // Early exit if no output function
    if (g_log_state.output_func == NULL) {
        return;
    }
    
    // Create log entry
    pico_rtos_log_entry_t entry;
    entry.timestamp = get_system_timestamp();
    entry.level = level;
    entry.subsystem = subsystem;
    entry.task_id = get_current_task_id();
    
    // Format the message
    va_list args;
    va_start(args, format);
    vsnprintf(entry.message, PICO_RTOS_LOG_MESSAGE_MAX_LENGTH, format, args);
    va_end(args);
    
    // Ensure null termination
    entry.message[PICO_RTOS_LOG_MESSAGE_MAX_LENGTH - 1] = '\0';
    
    // Call output function (this should be fast to avoid blocking)
    g_log_state.output_func(&entry);
}

const char *pico_rtos_log_level_to_string(pico_rtos_log_level_t level) {
    if (level >= 0 && level <= PICO_RTOS_LOG_LEVEL_DEBUG) {
        return g_log_level_strings[level];
    }
    return "INVALID";
}

const char *pico_rtos_log_subsystem_to_string(pico_rtos_log_subsystem_t subsystem) {
    return subsystem_to_string(subsystem);
}

// =============================================================================
// DEFAULT OUTPUT FUNCTIONS
// =============================================================================

/**
 * @brief Default log output function that prints to stdout
 * 
 * This function can be used as a default output handler for applications
 * that want simple console output.
 * 
 * @param entry Pointer to the log entry
 */
void pico_rtos_log_default_output(const pico_rtos_log_entry_t *entry) {
    if (entry == NULL) {
        return;
    }
    
    // Format: [TIMESTAMP] LEVEL SUBSYSTEM (TASK_ID): MESSAGE
    printf("[%010lu] %-5s %-9s (T%lu): %s\n",
           (unsigned long)entry->timestamp,
           pico_rtos_log_level_to_string(entry->level),
           pico_rtos_log_subsystem_to_string(entry->subsystem),
           (unsigned long)entry->task_id,
           entry->message);
}

/**
 * @brief Compact log output function for resource-constrained environments
 * 
 * This function provides a more compact output format to save memory and
 * reduce output bandwidth.
 * 
 * @param entry Pointer to the log entry
 */
void pico_rtos_log_compact_output(const pico_rtos_log_entry_t *entry) {
    if (entry == NULL) {
        return;
    }
    
    // Format: T:LEVEL:SUBSYS:MSG
    printf("%lu:%c:%c:%s\n",
           (unsigned long)entry->timestamp,
           pico_rtos_log_level_to_string(entry->level)[0],  // First letter of level
           pico_rtos_log_subsystem_to_string(entry->subsystem)[0],  // First letter of subsystem
           entry->message);
}

#endif // PICO_RTOS_ENABLE_LOGGING