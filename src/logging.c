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
    pico_rtos_log_output_func_t output_func;    ///< Primary output function pointer
    pico_rtos_log_output_handler_t output_handlers[PICO_RTOS_LOG_MAX_OUTPUT_HANDLERS]; ///< Multiple output handlers
    uint32_t num_output_handlers;               ///< Number of active output handlers
    pico_rtos_log_filter_func_t filter_func;   ///< Message filter function
    pico_rtos_log_config_t config;             ///< Current configuration
    pico_rtos_log_statistics_t stats;          ///< Logging statistics
    pico_rtos_log_level_t subsystem_levels[17]; ///< Per-subsystem log levels
    bool initialized;                           ///< Initialization flag
    spin_lock_t *lock;                          ///< Spinlock for thread safety
    
    // Rate limiting
    uint32_t rate_limit_counter;                ///< Rate limiting counter
    uint32_t rate_limit_last_reset;             ///< Last rate limit reset time
    
    // Buffering
    char log_buffer[PICO_RTOS_LOG_BUFFER_SIZE]; ///< Log buffer
    uint32_t buffer_pos;                        ///< Current buffer position
    uint32_t last_flush_time;                   ///< Last buffer flush time
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
    "CORE",         // PICO_RTOS_LOG_SUBSYSTEM_CORE = 0x01
    "TASK",         // PICO_RTOS_LOG_SUBSYSTEM_TASK = 0x02
    "MUTEX",        // PICO_RTOS_LOG_SUBSYSTEM_MUTEX = 0x04
    "QUEUE",        // PICO_RTOS_LOG_SUBSYSTEM_QUEUE = 0x08
    "TIMER",        // PICO_RTOS_LOG_SUBSYSTEM_TIMER = 0x10
    "MEMORY",       // PICO_RTOS_LOG_SUBSYSTEM_MEMORY = 0x20
    "SEMAPHORE",    // PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE = 0x40
    "EVENT_GROUP",  // PICO_RTOS_LOG_SUBSYSTEM_EVENT_GROUP = 0x80
    "SMP",          // PICO_RTOS_LOG_SUBSYSTEM_SMP = 0x100
    "STREAM_BUF",   // PICO_RTOS_LOG_SUBSYSTEM_STREAM_BUFFER = 0x200
    "MEM_POOL",     // PICO_RTOS_LOG_SUBSYSTEM_MEMORY_POOL = 0x400
    "IO",           // PICO_RTOS_LOG_SUBSYSTEM_IO = 0x800
    "HIRES_TIMER",  // PICO_RTOS_LOG_SUBSYSTEM_HIRES_TIMER = 0x1000
    "PROFILER",     // PICO_RTOS_LOG_SUBSYSTEM_PROFILER = 0x2000
    "TRACE",        // PICO_RTOS_LOG_SUBSYSTEM_TRACE = 0x4000
    "DEBUG",        // PICO_RTOS_LOG_SUBSYSTEM_DEBUG = 0x8000
    "USER"          // PICO_RTOS_LOG_SUBSYSTEM_USER = 0x10000
};

/**
 * @brief ANSI color codes for colored output
 */
static const char *g_log_level_colors[] = {
    "",           // NONE
    "\033[31m",   // ERROR - Red
    "\033[33m",   // WARN - Yellow
    "\033[32m",   // INFO - Green
    "\033[36m"    // DEBUG - Cyan
};

#define ANSI_RESET "\033[0m"

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
    
    // Initialize basic state
    g_log_state.output_func = output_func;
    g_log_state.current_level = PICO_RTOS_DEFAULT_LOG_LEVEL;
    g_log_state.enabled_subsystems = PICO_RTOS_DEFAULT_LOG_SUBSYSTEMS;
    g_log_state.num_output_handlers = 0;
    g_log_state.filter_func = NULL;
    
    // Initialize configuration with defaults
    g_log_state.config.global_level = PICO_RTOS_DEFAULT_LOG_LEVEL;
    g_log_state.config.enabled_subsystems = PICO_RTOS_DEFAULT_LOG_SUBSYSTEMS;
    g_log_state.config.timestamp_enabled = true;
    g_log_state.config.task_id_enabled = true;
    g_log_state.config.file_line_enabled = false;
    g_log_state.config.color_output_enabled = false;
    g_log_state.config.buffering_enabled = false;
    g_log_state.config.rate_limit_messages_per_second = 0;
    g_log_state.config.buffer_flush_interval_ms = 100;
    
    // Initialize statistics
    memset(&g_log_state.stats, 0, sizeof(g_log_state.stats));
    
    // Initialize per-subsystem levels to global level
    for (int i = 0; i < 17; i++) {
        g_log_state.subsystem_levels[i] = PICO_RTOS_DEFAULT_LOG_LEVEL;
    }
    
    // Initialize rate limiting
    g_log_state.rate_limit_counter = 0;
    g_log_state.rate_limit_last_reset = get_system_timestamp();
    
    // Initialize buffering
    g_log_state.buffer_pos = 0;
    g_log_state.last_flush_time = get_system_timestamp();
    
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

/**
 * @brief Colored log output function
 */
void pico_rtos_log_colored_output(const pico_rtos_log_entry_t *entry) {
    if (entry == NULL) {
        return;
    }
    
    const char *color = (entry->level <= PICO_RTOS_LOG_LEVEL_DEBUG) ? 
                       g_log_level_colors[entry->level] : "";
    
    printf("%s[%010lu] %-5s %-9s (T%lu): %s%s\n",
           color,
           (unsigned long)entry->timestamp,
           pico_rtos_log_level_to_string(entry->level),
           pico_rtos_log_subsystem_to_string(entry->subsystem),
           (unsigned long)entry->task_id,
           entry->message,
           ANSI_RESET);
}

/**
 * @brief JSON format log output function
 */
void pico_rtos_log_json_output(const pico_rtos_log_entry_t *entry) {
    if (entry == NULL) {
        return;
    }
    
    printf("{\"timestamp\":%lu,\"level\":\"%s\",\"subsystem\":\"%s\",\"task_id\":%lu,\"message\":\"%s\"}\n",
           (unsigned long)entry->timestamp,
           pico_rtos_log_level_to_string(entry->level),
           pico_rtos_log_subsystem_to_string(entry->subsystem),
           (unsigned long)entry->task_id,
           entry->message);
}

/**
 * @brief CSV format log output function
 */
void pico_rtos_log_csv_output(const pico_rtos_log_entry_t *entry) {
    if (entry == NULL) {
        return;
    }
    
    printf("%lu,%s,%s,%lu,\"%s\"\n",
           (unsigned long)entry->timestamp,
           pico_rtos_log_level_to_string(entry->level),
           pico_rtos_log_subsystem_to_string(entry->subsystem),
           (unsigned long)entry->task_id,
           entry->message);
}

// =============================================================================
// ADVANCED LOGGING FUNCTIONS
// =============================================================================

bool pico_rtos_log_add_output_handler(const pico_rtos_log_output_handler_t *handler) {
    if (!g_log_state.initialized || handler == NULL || handler->output_func == NULL) {
        return false;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    
    if (g_log_state.num_output_handlers >= PICO_RTOS_LOG_MAX_OUTPUT_HANDLERS) {
        spin_unlock(g_log_state.lock, save);
        return false;
    }
    
    g_log_state.output_handlers[g_log_state.num_output_handlers] = *handler;
    g_log_state.num_output_handlers++;
    
    spin_unlock(g_log_state.lock, save);
    return true;
}

bool pico_rtos_log_remove_output_handler(pico_rtos_log_output_func_t output_func) {
    if (!g_log_state.initialized || output_func == NULL) {
        return false;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    
    for (uint32_t i = 0; i < g_log_state.num_output_handlers; i++) {
        if (g_log_state.output_handlers[i].output_func == output_func) {
            // Shift remaining handlers down
            for (uint32_t j = i; j < g_log_state.num_output_handlers - 1; j++) {
                g_log_state.output_handlers[j] = g_log_state.output_handlers[j + 1];
            }
            g_log_state.num_output_handlers--;
            spin_unlock(g_log_state.lock, save);
            return true;
        }
    }
    
    spin_unlock(g_log_state.lock, save);
    return false;
}

void pico_rtos_log_set_filter_func(pico_rtos_log_filter_func_t filter_func) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.filter_func = filter_func;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_get_config(pico_rtos_log_config_t *config) {
    if (!g_log_state.initialized || config == NULL) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    *config = g_log_state.config;
    spin_unlock(g_log_state.lock, save);
}

bool pico_rtos_log_set_config(const pico_rtos_log_config_t *config) {
    if (!g_log_state.initialized || config == NULL) {
        return false;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config = *config;
    g_log_state.current_level = config->global_level;
    g_log_state.enabled_subsystems = config->enabled_subsystems;
    spin_unlock(g_log_state.lock, save);
    
    return true;
}

void pico_rtos_log_enable_timestamps(bool enable) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config.timestamp_enabled = enable;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_enable_task_ids(bool enable) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config.task_id_enabled = enable;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_enable_file_line(bool enable) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config.file_line_enabled = enable;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_enable_colors(bool enable) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config.color_output_enabled = enable;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_set_rate_limit(uint32_t messages_per_second) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config.rate_limit_messages_per_second = messages_per_second;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_enable_buffering(bool enable) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config.buffering_enabled = enable;
    if (!enable) {
        // Flush buffer when disabling
        pico_rtos_log_flush();
    }
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_set_flush_interval(uint32_t interval_ms) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    g_log_state.config.buffer_flush_interval_ms = interval_ms;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_flush(void) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    
    if (g_log_state.buffer_pos > 0 && g_log_state.output_func != NULL) {
        // Create a log entry for the buffered content
        pico_rtos_log_entry_t entry;
        entry.timestamp = get_system_timestamp();
        entry.level = PICO_RTOS_LOG_LEVEL_INFO;
        entry.subsystem = PICO_RTOS_LOG_SUBSYSTEM_CORE;
        entry.task_id = get_current_task_id();
        strncpy(entry.message, g_log_state.log_buffer, PICO_RTOS_LOG_MESSAGE_MAX_LENGTH - 1);
        entry.message[PICO_RTOS_LOG_MESSAGE_MAX_LENGTH - 1] = '\0';
        
        g_log_state.output_func(&entry);
        g_log_state.buffer_pos = 0;
        g_log_state.last_flush_time = get_system_timestamp();
    }
    
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_get_statistics(pico_rtos_log_statistics_t *stats) {
    if (!g_log_state.initialized || stats == NULL) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    *stats = g_log_state.stats;
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_reset_statistics(void) {
    if (!g_log_state.initialized) {
        return;
    }
    
    uint32_t save = spin_lock_blocking(g_log_state.lock);
    memset(&g_log_state.stats, 0, sizeof(g_log_state.stats));
    spin_unlock(g_log_state.lock, save);
}

void pico_rtos_log_set_subsystem_level(pico_rtos_log_subsystem_t subsystem, 
                                      pico_rtos_log_level_t level) {
    if (!g_log_state.initialized) {
        return;
    }
    
    // Find subsystem index
    int index = -1;
    for (int i = 0; i < 17; i++) {
        if (subsystem & (1 << i)) {
            index = i;
            break;
        }
    }
    
    if (index >= 0) {
        uint32_t save = spin_lock_blocking(g_log_state.lock);
        g_log_state.subsystem_levels[index] = level;
        spin_unlock(g_log_state.lock, save);
    }
}

pico_rtos_log_level_t pico_rtos_log_get_subsystem_level(pico_rtos_log_subsystem_t subsystem) {
    if (!g_log_state.initialized) {
        return PICO_RTOS_LOG_LEVEL_NONE;
    }
    
    // Find subsystem index
    int index = -1;
    for (int i = 0; i < 17; i++) {
        if (subsystem & (1 << i)) {
            index = i;
            break;
        }
    }
    
    if (index >= 0) {
        uint32_t save = spin_lock_blocking(g_log_state.lock);
        pico_rtos_log_level_t level = g_log_state.subsystem_levels[index];
        spin_unlock(g_log_state.lock, save);
        return level;
    }
    
    return PICO_RTOS_LOG_LEVEL_NONE;
}

void pico_rtos_log_vprintf(pico_rtos_log_level_t level,
                          pico_rtos_log_subsystem_t subsystem,
                          const char *format,
                          va_list args) {
    if (!g_log_state.initialized) {
        return;
    }
    
    // Create log entry
    pico_rtos_log_entry_t entry;
    entry.timestamp = get_system_timestamp();
    entry.level = level;
    entry.subsystem = subsystem;
    entry.task_id = get_current_task_id();
    
    // Format the message
    vsnprintf(entry.message, PICO_RTOS_LOG_MESSAGE_MAX_LENGTH, format, args);
    entry.message[PICO_RTOS_LOG_MESSAGE_MAX_LENGTH - 1] = '\0';
    
    // Process the log entry through normal channels
    if (g_log_state.output_func != NULL) {
        g_log_state.output_func(&entry);
    }
}

void pico_rtos_log_hex_dump(pico_rtos_log_level_t level,
                           pico_rtos_log_subsystem_t subsystem,
                           const void *data,
                           size_t length,
                           const char *description) {
    if (!g_log_state.initialized || data == NULL) {
        return;
    }
    
    const uint8_t *bytes = (const uint8_t *)data;
    char hex_line[80];
    
    // Log description header
    pico_rtos_log(level, subsystem, __FILE__, __LINE__, 
                  "Hex dump: %s (%zu bytes)", description ? description : "Data", length);
    
    // Dump data in 16-byte lines
    for (size_t i = 0; i < length; i += 16) {
        int pos = 0;
        
        // Address
        pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%04zx: ", i);
        
        // Hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < length) {
                pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%02x ", bytes[i + j]);
            } else {
                pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "   ");
            }
        }
        
        // ASCII representation
        pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, " |");
        for (size_t j = 0; j < 16 && i + j < length; j++) {
            char c = bytes[i + j];
            pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%c", 
                           (c >= 32 && c <= 126) ? c : '.');
        }
        pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "|");
        
        pico_rtos_log(level, subsystem, __FILE__, __LINE__, "%s", hex_line);
    }
}

#endif // PICO_RTOS_ENABLE_LOGGING