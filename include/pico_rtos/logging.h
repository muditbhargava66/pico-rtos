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
    PICO_RTOS_LOG_SUBSYSTEM_CORE = 0x01,    ///< Core scheduler and system functions
    PICO_RTOS_LOG_SUBSYSTEM_TASK = 0x02,    ///< Task management functions
    PICO_RTOS_LOG_SUBSYSTEM_MUTEX = 0x04,   ///< Mutex operations
    PICO_RTOS_LOG_SUBSYSTEM_QUEUE = 0x08,   ///< Queue operations
    PICO_RTOS_LOG_SUBSYSTEM_TIMER = 0x10,   ///< Timer operations
    PICO_RTOS_LOG_SUBSYSTEM_MEMORY = 0x20,  ///< Memory management
    PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE = 0x40, ///< Semaphore operations
    PICO_RTOS_LOG_SUBSYSTEM_ALL = 0xFF      ///< All subsystems
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

#ifdef __cplusplus
}
#endif

#endif // PICO_RTOS_LOGGING_H