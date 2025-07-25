#ifndef PICO_RTOS_LOGGING_H
#define PICO_RTOS_LOGGING_H

/**
 * @file logging.h
 * @brief Pico-RTOS Debug Logging System
 * 
 * This file provides a configurable debug logging system for Pico-RTOS.
 * The logging system supports multiple log levels and subsystems, and
 * can be completely disabled at compile time for zero runtime overhead.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// LOG LEVEL DEFINITIONS
// =============================================================================

/**
 * @brief Log level enumeration
 * 
 * Defines the severity levels for log messages. Higher values indicate
 * more verbose logging.
 */
typedef enum {
    PICO_RTOS_LOG_LEVEL_NONE = 0,   ///< No logging
    PICO_RTOS_LOG_LEVEL_ERROR = 1,  ///< Error conditions only
    PICO_RTOS_LOG_LEVEL_WARN = 2,   ///< Warnings and errors
    PICO_RTOS_LOG_LEVEL_INFO = 3,   ///< Informational messages, warnings, and errors
    PICO_RTOS_LOG_LEVEL_DEBUG = 4   ///< All messages including debug information
} pico_rtos_log_level_t;

// =============================================================================
// SUBSYSTEM DEFINITIONS
// =============================================================================

/**
 * @brief Log subsystem flags
 * 
 * Defines the different RTOS subsystems that can generate log messages.
 * Multiple subsystems can be enabled simultaneously using bitwise OR.
 */
typedef enum {
    PICO_RTOS_LOG_SUBSYSTEM_CORE = 0x01,        ///< Core scheduler and system functions
    PICO_RTOS_LOG_SUBSYSTEM_TASK = 0x02,        ///< Task management functions
    PICO_RTOS_LOG_SUBSYSTEM_MUTEX = 0x04,       ///< Mutex operations
    PICO_RTOS_LOG_SUBSYSTEM_QUEUE = 0x08,       ///< Queue operations
    PICO_RTOS_LOG_SUBSYSTEM_TIMER = 0x10,       ///< Timer operations
    PICO_RTOS_LOG_SUBSYSTEM_MEMORY = 0x20,      ///< Memory management
    PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE = 0x40,   ///< Semaphore operations
    PICO_RTOS_LOG_SUBSYSTEM_EVENT_GROUP = 0x80, ///< Event group operations
    PICO_RTOS_LOG_SUBSYSTEM_SMP = 0x100,        ///< SMP multi-core operations
    PICO_RTOS_LOG_SUBSYSTEM_STREAM_BUFFER = 0x200, ///< Stream buffer operations
    PICO_RTOS_LOG_SUBSYSTEM_MEMORY_POOL = 0x400,   ///< Memory pool operations
    PICO_RTOS_LOG_SUBSYSTEM_IO = 0x800,         ///< I/O abstraction layer
    PICO_RTOS_LOG_SUBSYSTEM_HIRES_TIMER = 0x1000, ///< High-resolution timers
    PICO_RTOS_LOG_SUBSYSTEM_PROFILER = 0x2000,  ///< Profiling subsystem
    PICO_RTOS_LOG_SUBSYSTEM_TRACE = 0x4000,     ///< Tracing subsystem
    PICO_RTOS_LOG_SUBSYSTEM_DEBUG = 0x8000,     ///< Debug subsystem
    PICO_RTOS_LOG_SUBSYSTEM_USER = 0x10000,     ///< User-defined subsystem
    PICO_RTOS_LOG_SUBSYSTEM_ALL = 0x1FFFF       ///< All subsystems
} pico_rtos_log_subsystem_t;

// =============================================================================
// CONFIGURATION CONSTANTS
// =============================================================================

/**
 * @brief Maximum length of a log message
 * 
 * This includes the formatted message text but not the timestamp,
 * level, and subsystem information.
 */
#ifndef PICO_RTOS_LOG_MESSAGE_MAX_LENGTH
#define PICO_RTOS_LOG_MESSAGE_MAX_LENGTH 128
#endif

/**
 * @brief Default log level when logging is enabled
 */
#ifndef PICO_RTOS_DEFAULT_LOG_LEVEL
#define PICO_RTOS_DEFAULT_LOG_LEVEL PICO_RTOS_LOG_LEVEL_INFO
#endif

/**
 * @brief Default enabled subsystems when logging is enabled
 */
#ifndef PICO_RTOS_DEFAULT_LOG_SUBSYSTEMS
#define PICO_RTOS_DEFAULT_LOG_SUBSYSTEMS PICO_RTOS_LOG_SUBSYSTEM_ALL
#endif

/**
 * @brief Maximum number of log output handlers
 */
#ifndef PICO_RTOS_LOG_MAX_OUTPUT_HANDLERS
#define PICO_RTOS_LOG_MAX_OUTPUT_HANDLERS 4
#endif

/**
 * @brief Log buffer size for buffered logging
 */
#ifndef PICO_RTOS_LOG_BUFFER_SIZE
#define PICO_RTOS_LOG_BUFFER_SIZE 1024
#endif

/**
 * @brief Enable log message filtering by content
 */
#ifndef PICO_RTOS_LOG_ENABLE_FILTERING
#define PICO_RTOS_LOG_ENABLE_FILTERING 1
#endif

/**
 * @brief Enable log message rate limiting
 */
#ifndef PICO_RTOS_LOG_ENABLE_RATE_LIMITING
#define PICO_RTOS_LOG_ENABLE_RATE_LIMITING 1
#endif

/**
 * @brief Enable log message buffering
 */
#ifndef PICO_RTOS_LOG_ENABLE_BUFFERING
#define PICO_RTOS_LOG_ENABLE_BUFFERING 1
#endif

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Log entry structure
 * 
 * Contains all information for a single log entry.
 */
typedef struct {
    uint32_t timestamp;                                     ///< System timestamp in ticks
    pico_rtos_log_level_t level;                           ///< Log level
    pico_rtos_log_subsystem_t subsystem;                   ///< Originating subsystem
    uint32_t task_id;                                      ///< Task ID (0 for ISR context)
    char message[PICO_RTOS_LOG_MESSAGE_MAX_LENGTH];        ///< Formatted message
} pico_rtos_log_entry_t;

/**
 * @brief Log output function pointer type
 * 
 * User-provided function to handle log output. This function will be called
 * for each log message that passes the level and subsystem filters.
 * 
 * @param entry Pointer to the log entry structure
 */
typedef void (*pico_rtos_log_output_func_t)(const pico_rtos_log_entry_t *entry);

/**
 * @brief Log filter function pointer type
 * 
 * User-provided function to filter log messages based on content.
 * 
 * @param entry Pointer to the log entry structure
 * @return true if message should be logged, false to filter out
 */
typedef bool (*pico_rtos_log_filter_func_t)(const pico_rtos_log_entry_t *entry);

/**
 * @brief Log output handler structure
 */
typedef struct {
    pico_rtos_log_output_func_t output_func;    ///< Output function
    pico_rtos_log_level_t min_level;            ///< Minimum level for this handler
    uint32_t subsystem_mask;                    ///< Enabled subsystems for this handler
    bool enabled;                               ///< Handler enabled flag
    const char *name;                           ///< Handler name (for debugging)
} pico_rtos_log_output_handler_t;

/**
 * @brief Log statistics structure
 */
typedef struct {
    uint32_t total_messages;                    ///< Total messages processed
    uint32_t messages_by_level[5];              ///< Messages by level (indexed by log level)
    uint32_t messages_filtered;                 ///< Messages filtered out
    uint32_t messages_rate_limited;             ///< Messages dropped due to rate limiting
    uint32_t buffer_overflows;                  ///< Buffer overflow events
    uint32_t output_errors;                     ///< Output function errors
    uint64_t total_processing_time_us;          ///< Total time spent processing logs
    uint32_t max_message_length;                ///< Maximum message length seen
} pico_rtos_log_statistics_t;

/**
 * @brief Log configuration structure
 */
typedef struct {
    pico_rtos_log_level_t global_level;         ///< Global log level
    uint32_t enabled_subsystems;                ///< Enabled subsystem mask
    bool timestamp_enabled;                     ///< Include timestamps in logs
    bool task_id_enabled;                       ///< Include task IDs in logs
    bool file_line_enabled;                     ///< Include file/line info in logs
    bool color_output_enabled;                  ///< Enable colored output
    bool buffering_enabled;                     ///< Enable log buffering
    uint32_t rate_limit_messages_per_second;    ///< Rate limit (0 = disabled)
    uint32_t buffer_flush_interval_ms;          ///< Buffer flush interval
} pico_rtos_log_config_t;

// =============================================================================
// LOGGING FUNCTIONS (only compiled when logging is enabled)
// =============================================================================

#if PICO_RTOS_ENABLE_LOGGING

/**
 * @brief Initialize the logging system
 * 
 * Must be called before using any logging functions. Sets up default
 * configuration and prepares the logging system for use.
 * 
 * @param output_func Function to handle log output (can be NULL for no output)
 */
void pico_rtos_log_init(pico_rtos_log_output_func_t output_func);

/**
 * @brief Set the current log level
 * 
 * Only messages at or below this level will be processed.
 * 
 * @param level New log level
 */
void pico_rtos_log_set_level(pico_rtos_log_level_t level);

/**
 * @brief Get the current log level
 * 
 * @return Current log level
 */
pico_rtos_log_level_t pico_rtos_log_get_level(void);

/**
 * @brief Enable logging for specific subsystems
 * 
 * @param subsystem_mask Bitwise OR of subsystems to enable
 */
void pico_rtos_log_enable_subsystem(uint32_t subsystem_mask);

/**
 * @brief Disable logging for specific subsystems
 * 
 * @param subsystem_mask Bitwise OR of subsystems to disable
 */
void pico_rtos_log_disable_subsystem(uint32_t subsystem_mask);

/**
 * @brief Check if a subsystem is enabled for logging
 * 
 * @param subsystem Subsystem to check
 * @return true if enabled, false otherwise
 */
bool pico_rtos_log_is_subsystem_enabled(pico_rtos_log_subsystem_t subsystem);

/**
 * @brief Set the log output function
 * 
 * @param output_func New output function (can be NULL to disable output)
 */
void pico_rtos_log_set_output_func(pico_rtos_log_output_func_t output_func);

/**
 * @brief Core logging function
 * 
 * This function is typically not called directly. Use the logging macros instead.
 * 
 * @param level Log level
 * @param subsystem Originating subsystem
 * @param file Source file name
 * @param line Source line number
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void pico_rtos_log(pico_rtos_log_level_t level, 
                   pico_rtos_log_subsystem_t subsystem,
                   const char *file, 
                   int line, 
                   const char *format, 
                   ...);

/**
 * @brief Get string representation of log level
 * 
 * @param level Log level
 * @return String representation
 */
const char *pico_rtos_log_level_to_string(pico_rtos_log_level_t level);

/**
 * @brief Get string representation of subsystem
 * 
 * @param subsystem Subsystem
 * @return String representation
 */
const char *pico_rtos_log_subsystem_to_string(pico_rtos_log_subsystem_t subsystem);

/**
 * @brief Default log output function that prints to stdout
 * 
 * This function can be used as a default output handler for applications
 * that want simple console output.
 * 
 * @param entry Pointer to the log entry
 */
void pico_rtos_log_default_output(const pico_rtos_log_entry_t *entry);

/**
 * @brief Compact log output function for resource-constrained environments
 * 
 * This function provides a more compact output format to save memory and
 * reduce output bandwidth.
 * 
 * @param entry Pointer to the log entry
 */
void pico_rtos_log_compact_output(const pico_rtos_log_entry_t *entry);

/**
 * @brief Add a log output handler
 * 
 * @param handler Pointer to output handler structure
 * @return true if handler added successfully, false otherwise
 */
bool pico_rtos_log_add_output_handler(const pico_rtos_log_output_handler_t *handler);

/**
 * @brief Remove a log output handler
 * 
 * @param output_func Output function to remove
 * @return true if handler removed successfully, false otherwise
 */
bool pico_rtos_log_remove_output_handler(pico_rtos_log_output_func_t output_func);

/**
 * @brief Set log filter function
 * 
 * @param filter_func Filter function (NULL to disable filtering)
 */
void pico_rtos_log_set_filter_func(pico_rtos_log_filter_func_t filter_func);

/**
 * @brief Get current log configuration
 * 
 * @param config Pointer to configuration structure to fill
 */
void pico_rtos_log_get_config(pico_rtos_log_config_t *config);

/**
 * @brief Set log configuration
 * 
 * @param config Pointer to new configuration
 * @return true if configuration applied successfully, false otherwise
 */
bool pico_rtos_log_set_config(const pico_rtos_log_config_t *config);

/**
 * @brief Enable/disable timestamps in log messages
 * 
 * @param enable true to enable timestamps, false to disable
 */
void pico_rtos_log_enable_timestamps(bool enable);

/**
 * @brief Enable/disable task IDs in log messages
 * 
 * @param enable true to enable task IDs, false to disable
 */
void pico_rtos_log_enable_task_ids(bool enable);

/**
 * @brief Enable/disable file/line information in log messages
 * 
 * @param enable true to enable file/line info, false to disable
 */
void pico_rtos_log_enable_file_line(bool enable);

/**
 * @brief Enable/disable colored output
 * 
 * @param enable true to enable colors, false to disable
 */
void pico_rtos_log_enable_colors(bool enable);

/**
 * @brief Set rate limiting for log messages
 * 
 * @param messages_per_second Maximum messages per second (0 to disable)
 */
void pico_rtos_log_set_rate_limit(uint32_t messages_per_second);

/**
 * @brief Enable/disable log buffering
 * 
 * @param enable true to enable buffering, false for immediate output
 */
void pico_rtos_log_enable_buffering(bool enable);

/**
 * @brief Set buffer flush interval
 * 
 * @param interval_ms Flush interval in milliseconds
 */
void pico_rtos_log_set_flush_interval(uint32_t interval_ms);

/**
 * @brief Manually flush log buffer
 */
void pico_rtos_log_flush(void);

/**
 * @brief Get log statistics
 * 
 * @param stats Pointer to statistics structure to fill
 */
void pico_rtos_log_get_statistics(pico_rtos_log_statistics_t *stats);

/**
 * @brief Reset log statistics
 */
void pico_rtos_log_reset_statistics(void);

/**
 * @brief Set per-subsystem log level
 * 
 * @param subsystem Subsystem to configure
 * @param level Log level for this subsystem
 */
void pico_rtos_log_set_subsystem_level(pico_rtos_log_subsystem_t subsystem, 
                                      pico_rtos_log_level_t level);

/**
 * @brief Get per-subsystem log level
 * 
 * @param subsystem Subsystem to query
 * @return Log level for this subsystem
 */
pico_rtos_log_level_t pico_rtos_log_get_subsystem_level(pico_rtos_log_subsystem_t subsystem);

/**
 * @brief Log a message with custom formatting
 * 
 * @param level Log level
 * @param subsystem Originating subsystem
 * @param format Printf-style format string
 * @param args Variable argument list
 */
void pico_rtos_log_vprintf(pico_rtos_log_level_t level,
                          pico_rtos_log_subsystem_t subsystem,
                          const char *format,
                          va_list args);

/**
 * @brief Log raw data as hexadecimal dump
 * 
 * @param level Log level
 * @param subsystem Originating subsystem
 * @param data Pointer to data to dump
 * @param length Number of bytes to dump
 * @param description Description of the data
 */
void pico_rtos_log_hex_dump(pico_rtos_log_level_t level,
                           pico_rtos_log_subsystem_t subsystem,
                           const void *data,
                           size_t length,
                           const char *description);

/**
 * @brief Colored log output function
 * 
 * @param entry Pointer to the log entry
 */
void pico_rtos_log_colored_output(const pico_rtos_log_entry_t *entry);

/**
 * @brief JSON format log output function
 * 
 * @param entry Pointer to the log entry
 */
void pico_rtos_log_json_output(const pico_rtos_log_entry_t *entry);

/**
 * @brief CSV format log output function
 * 
 * @param entry Pointer to the log entry
 */
void pico_rtos_log_csv_output(const pico_rtos_log_entry_t *entry);

#endif // PICO_RTOS_ENABLE_LOGGING

// =============================================================================
// LOGGING MACROS
// =============================================================================

#if PICO_RTOS_ENABLE_LOGGING

/**
 * @brief Log an error message
 * 
 * @param subsystem Originating subsystem
 * @param format Printf-style format string
 * @param ... Format arguments
 */
#define PICO_RTOS_LOG_ERROR(subsystem, format, ...) \
    pico_rtos_log(PICO_RTOS_LOG_LEVEL_ERROR, subsystem, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Log a warning message
 * 
 * @param subsystem Originating subsystem
 * @param format Printf-style format string
 * @param ... Format arguments
 */
#define PICO_RTOS_LOG_WARN(subsystem, format, ...) \
    pico_rtos_log(PICO_RTOS_LOG_LEVEL_WARN, subsystem, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Log an informational message
 * 
 * @param subsystem Originating subsystem
 * @param format Printf-style format string
 * @param ... Format arguments
 */
#define PICO_RTOS_LOG_INFO(subsystem, format, ...) \
    pico_rtos_log(PICO_RTOS_LOG_LEVEL_INFO, subsystem, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Log a debug message
 * 
 * @param subsystem Originating subsystem
 * @param format Printf-style format string
 * @param ... Format arguments
 */
#define PICO_RTOS_LOG_DEBUG(subsystem, format, ...) \
    pico_rtos_log(PICO_RTOS_LOG_LEVEL_DEBUG, subsystem, __FILE__, __LINE__, format, ##__VA_ARGS__)

#else

// When logging is disabled, all macros compile to nothing (zero overhead)
#define PICO_RTOS_LOG_ERROR(subsystem, format, ...)
#define PICO_RTOS_LOG_WARN(subsystem, format, ...)
#define PICO_RTOS_LOG_INFO(subsystem, format, ...)
#define PICO_RTOS_LOG_DEBUG(subsystem, format, ...)

#endif // PICO_RTOS_ENABLE_LOGGING

// =============================================================================
// CONVENIENCE MACROS FOR COMMON SUBSYSTEMS
// =============================================================================

#define PICO_RTOS_LOG_CORE_ERROR(format, ...)   PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_CORE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_CORE_WARN(format, ...)    PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_CORE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_CORE_INFO(format, ...)    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_CORE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_CORE_DEBUG(format, ...)   PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_CORE, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_TASK_ERROR(format, ...)   PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TASK, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TASK_WARN(format, ...)    PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_TASK, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TASK_INFO(format, ...)    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_TASK, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TASK_DEBUG(format, ...)   PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_TASK, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_MUTEX_ERROR(format, ...)  PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_MUTEX, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_MUTEX_WARN(format, ...)   PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MUTEX, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_MUTEX_INFO(format, ...)   PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_MUTEX, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_MUTEX_DEBUG(format, ...)  PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MUTEX, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_QUEUE_ERROR(format, ...)  PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_QUEUE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_QUEUE_WARN(format, ...)   PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_QUEUE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_QUEUE_INFO(format, ...)   PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_QUEUE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_QUEUE_DEBUG(format, ...)  PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_QUEUE, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_TIMER_ERROR(format, ...)  PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TIMER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TIMER_WARN(format, ...)   PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_TIMER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TIMER_INFO(format, ...)   PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_TIMER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TIMER_DEBUG(format, ...)  PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_TIMER, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_MEMORY_ERROR(format, ...) PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_MEMORY_WARN(format, ...)  PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_MEMORY_INFO(format, ...)  PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_MEMORY_DEBUG(format, ...) PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_SEM_ERROR(format, ...)    PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_SEM_WARN(format, ...)     PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_SEM_INFO(format, ...)     PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_SEM_DEBUG(format, ...)    PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_EVENT_ERROR(format, ...)  PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_EVENT_GROUP, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_EVENT_WARN(format, ...)   PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_EVENT_GROUP, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_EVENT_INFO(format, ...)   PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_EVENT_GROUP, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_EVENT_DEBUG(format, ...)  PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_EVENT_GROUP, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_SMP_ERROR(format, ...)    PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_SMP, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_SMP_WARN(format, ...)     PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_SMP, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_SMP_INFO(format, ...)     PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_SMP, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_SMP_DEBUG(format, ...)    PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_SMP, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_STREAM_ERROR(format, ...) PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_STREAM_BUFFER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_STREAM_WARN(format, ...)  PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_STREAM_BUFFER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_STREAM_INFO(format, ...)  PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_STREAM_BUFFER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_STREAM_DEBUG(format, ...) PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_STREAM_BUFFER, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_POOL_ERROR(format, ...)   PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_MEMORY_POOL, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_POOL_WARN(format, ...)    PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY_POOL, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_POOL_INFO(format, ...)    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_MEMORY_POOL, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_POOL_DEBUG(format, ...)   PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MEMORY_POOL, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_IO_ERROR(format, ...)     PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_IO, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_IO_WARN(format, ...)      PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_IO, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_IO_INFO(format, ...)      PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_IO, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_IO_DEBUG(format, ...)     PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_IO, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_HIRES_ERROR(format, ...)  PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_HIRES_TIMER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_HIRES_WARN(format, ...)   PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_HIRES_TIMER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_HIRES_INFO(format, ...)   PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_HIRES_TIMER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_HIRES_DEBUG(format, ...)  PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_HIRES_TIMER, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_PROF_ERROR(format, ...)   PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_PROFILER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_PROF_WARN(format, ...)    PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_PROFILER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_PROF_INFO(format, ...)    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_PROFILER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_PROF_DEBUG(format, ...)   PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_PROFILER, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_TRACE_ERROR(format, ...)  PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TRACE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TRACE_WARN(format, ...)   PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_TRACE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TRACE_INFO(format, ...)   PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_TRACE, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_TRACE_DEBUG(format, ...)  PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_TRACE, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_DBG_ERROR(format, ...)    PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_DEBUG, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_DBG_WARN(format, ...)     PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_DEBUG, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_DBG_INFO(format, ...)     PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_DEBUG, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_DBG_DEBUG(format, ...)    PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_DEBUG, format, ##__VA_ARGS__)

#define PICO_RTOS_LOG_USER_ERROR(format, ...)   PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_USER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_USER_WARN(format, ...)    PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_USER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_USER_INFO(format, ...)    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_USER, format, ##__VA_ARGS__)
#define PICO_RTOS_LOG_USER_DEBUG(format, ...)   PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_USER, format, ##__VA_ARGS__)

// Simplified macros that don't require subsystem specification (only define if not already defined)
#ifndef PICO_RTOS_LOG_ERROR
#define PICO_RTOS_LOG_ERROR(format, ...)        PICO_RTOS_LOG_USER_ERROR(format, ##__VA_ARGS__)
#endif
#ifndef PICO_RTOS_LOG_WARN  
#define PICO_RTOS_LOG_WARN(format, ...)         PICO_RTOS_LOG_USER_WARN(format, ##__VA_ARGS__)
#endif
#ifndef PICO_RTOS_LOG_INFO
#define PICO_RTOS_LOG_INFO(format, ...)         PICO_RTOS_LOG_USER_INFO(format, ##__VA_ARGS__)
#endif
#ifndef PICO_RTOS_LOG_DEBUG
#define PICO_RTOS_LOG_DEBUG(format, ...)        PICO_RTOS_LOG_USER_DEBUG(format, ##__VA_ARGS__)
#endif

// Hex dump macros
#define PICO_RTOS_LOG_HEX_DUMP_ERROR(data, len, desc) \
    pico_rtos_log_hex_dump(PICO_RTOS_LOG_LEVEL_ERROR, PICO_RTOS_LOG_SUBSYSTEM_USER, data, len, desc)
#define PICO_RTOS_LOG_HEX_DUMP_WARN(data, len, desc) \
    pico_rtos_log_hex_dump(PICO_RTOS_LOG_LEVEL_WARN, PICO_RTOS_LOG_SUBSYSTEM_USER, data, len, desc)
#define PICO_RTOS_LOG_HEX_DUMP_INFO(data, len, desc) \
    pico_rtos_log_hex_dump(PICO_RTOS_LOG_LEVEL_INFO, PICO_RTOS_LOG_SUBSYSTEM_USER, data, len, desc)
#define PICO_RTOS_LOG_HEX_DUMP_DEBUG(data, len, desc) \
    pico_rtos_log_hex_dump(PICO_RTOS_LOG_LEVEL_DEBUG, PICO_RTOS_LOG_SUBSYSTEM_USER, data, len, desc)

#ifdef __cplusplus
}
#endif

#endif // PICO_RTOS_LOGGING_H