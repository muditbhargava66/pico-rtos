#ifndef PICO_RTOS_ERROR_H
#define PICO_RTOS_ERROR_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file error.h
 * @brief Enhanced Error Reporting System for Pico-RTOS
 * 
 * This module provides comprehensive error reporting with detailed error codes,
 * context information, and optional error history tracking for debugging.
 */

// =============================================================================
// ERROR CODE DEFINITIONS
// =============================================================================

/**
 * @brief Comprehensive error codes for Pico-RTOS
 * 
 * Error codes are organized by category with specific ranges:
 * - 0: No error
 * - 100-199: Task-related errors
 * - 200-299: Memory-related errors
 * - 300-399: Synchronization-related errors
 * - 400-499: System-related errors
 * - 500-599: Hardware-related errors
 * - 600-699: Configuration-related errors
 */
typedef enum {
    PICO_RTOS_ERROR_NONE = 0,
    
    // Task errors (100-199)
    PICO_RTOS_ERROR_TASK_INVALID_PRIORITY = 100,
    PICO_RTOS_ERROR_TASK_STACK_OVERFLOW = 101,
    PICO_RTOS_ERROR_TASK_INVALID_STATE = 102,
    PICO_RTOS_ERROR_TASK_INVALID_PARAMETER = 103,
    PICO_RTOS_ERROR_TASK_STACK_TOO_SMALL = 104,
    PICO_RTOS_ERROR_TASK_FUNCTION_NULL = 105,
    PICO_RTOS_ERROR_TASK_NOT_FOUND = 106,
    PICO_RTOS_ERROR_TASK_ALREADY_TERMINATED = 107,
    PICO_RTOS_ERROR_TASK_CANNOT_SUSPEND_SELF = 108,
    PICO_RTOS_ERROR_TASK_MAX_TASKS_EXCEEDED = 109,
    
    // Memory errors (200-299)
    PICO_RTOS_ERROR_OUT_OF_MEMORY = 200,
    PICO_RTOS_ERROR_INVALID_POINTER = 201,
    PICO_RTOS_ERROR_MEMORY_CORRUPTION = 202,
    PICO_RTOS_ERROR_MEMORY_ALIGNMENT = 203,
    PICO_RTOS_ERROR_MEMORY_DOUBLE_FREE = 204,
    PICO_RTOS_ERROR_MEMORY_LEAK_DETECTED = 205,
    PICO_RTOS_ERROR_MEMORY_FRAGMENTATION = 206,
    PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED = 207,
    
    // Synchronization errors (300-399)
    PICO_RTOS_ERROR_MUTEX_TIMEOUT = 300,
    PICO_RTOS_ERROR_MUTEX_NOT_OWNED = 301,
    PICO_RTOS_ERROR_MUTEX_RECURSIVE_LOCK = 302,
    PICO_RTOS_ERROR_SEMAPHORE_TIMEOUT = 310,
    PICO_RTOS_ERROR_SEMAPHORE_OVERFLOW = 311,
    PICO_RTOS_ERROR_SEMAPHORE_INVALID_COUNT = 312,
    PICO_RTOS_ERROR_QUEUE_FULL = 320,
    PICO_RTOS_ERROR_QUEUE_EMPTY = 321,
    PICO_RTOS_ERROR_QUEUE_TIMEOUT = 322,
    PICO_RTOS_ERROR_QUEUE_INVALID_SIZE = 323,
    PICO_RTOS_ERROR_QUEUE_INVALID_ITEM_SIZE = 324,
    PICO_RTOS_ERROR_TIMER_INVALID_PERIOD = 330,
    PICO_RTOS_ERROR_TIMER_NOT_RUNNING = 331,
    PICO_RTOS_ERROR_TIMER_ALREADY_RUNNING = 332,
    PICO_RTOS_ERROR_EVENT_GROUP_INVALID = 340,
    PICO_RTOS_ERROR_EVENT_GROUP_TIMEOUT = 341,
    PICO_RTOS_ERROR_EVENT_GROUP_DELETED = 342,
    
    // System errors (400-499)
    PICO_RTOS_ERROR_SYSTEM_NOT_INITIALIZED = 400,
    PICO_RTOS_ERROR_SYSTEM_ALREADY_INITIALIZED = 401,
    PICO_RTOS_ERROR_INVALID_CONFIGURATION = 402,
    PICO_RTOS_ERROR_SCHEDULER_NOT_RUNNING = 403,
    PICO_RTOS_ERROR_SCHEDULER_ALREADY_RUNNING = 404,
    PICO_RTOS_ERROR_CRITICAL_SECTION_VIOLATION = 405,
    PICO_RTOS_ERROR_INTERRUPT_CONTEXT_VIOLATION = 406,
    PICO_RTOS_ERROR_SYSTEM_OVERLOAD = 407,
    PICO_RTOS_ERROR_WATCHDOG_TIMEOUT = 408,
    
    // Hardware errors (500-599)
    PICO_RTOS_ERROR_HARDWARE_FAULT = 500,
    PICO_RTOS_ERROR_HARDWARE_TIMER_FAULT = 501,
    PICO_RTOS_ERROR_HARDWARE_INTERRUPT_FAULT = 502,
    PICO_RTOS_ERROR_HARDWARE_CLOCK_FAULT = 503,
    PICO_RTOS_ERROR_HARDWARE_MEMORY_FAULT = 504,
    PICO_RTOS_ERROR_HARDWARE_BUS_FAULT = 505,
    PICO_RTOS_ERROR_HARDWARE_USAGE_FAULT = 506,
    
    // Configuration errors (600-699)
    PICO_RTOS_ERROR_CONFIG_INVALID_TICK_RATE = 600,
    PICO_RTOS_ERROR_CONFIG_INVALID_STACK_SIZE = 601,
    PICO_RTOS_ERROR_CONFIG_INVALID_PRIORITY = 602,
    PICO_RTOS_ERROR_CONFIG_FEATURE_DISABLED = 603,
    PICO_RTOS_ERROR_CONFIG_DEPENDENCY_MISSING = 604,

    // Stream Buffer errors (merged from stream_buffer.h)
    PICO_RTOS_STREAM_BUFFER_ERROR_NONE = 0,
    PICO_RTOS_STREAM_BUFFER_ERROR_INVALID_PARAM = 220,
    PICO_RTOS_STREAM_BUFFER_ERROR_BUFFER_FULL = 221,
    PICO_RTOS_STREAM_BUFFER_ERROR_BUFFER_EMPTY = 222,
    PICO_RTOS_STREAM_BUFFER_ERROR_MESSAGE_TOO_LARGE = 223,
    PICO_RTOS_STREAM_BUFFER_ERROR_TIMEOUT = 224,
    PICO_RTOS_STREAM_BUFFER_ERROR_DELETED = 225,
    PICO_RTOS_STREAM_BUFFER_ERROR_ZERO_COPY_ACTIVE = 226
} pico_rtos_error_t;

// =============================================================================
// ERROR INFORMATION STRUCTURE
// =============================================================================

/**
 * @brief Detailed error information structure
 * 
 * Contains comprehensive information about an error including context,
 * timing, and location information for debugging purposes.
 */
typedef struct {
    pico_rtos_error_t code;          ///< Error code
    uint32_t timestamp;              ///< System tick when error occurred
    uint32_t task_id;                ///< ID of task where error occurred (0 if system/ISR)
    const char *file;                ///< Source file where error was reported
    int line;                        ///< Line number where error was reported
    const char *function;            ///< Function name where error occurred
    const char *description;         ///< Human-readable error description
    uint32_t context_data;           ///< Additional context-specific data
} pico_rtos_error_info_t;

// =============================================================================
// ERROR HISTORY MANAGEMENT
// =============================================================================

#if PICO_RTOS_ENABLE_ERROR_HISTORY

/**
 * @brief Error history entry for circular buffer
 */
typedef struct pico_rtos_error_entry {
    pico_rtos_error_info_t info;
    struct pico_rtos_error_entry *next;
} pico_rtos_error_entry_t;

/**
 * @brief Error history management structure
 */
typedef struct {
    pico_rtos_error_entry_t *head;
    pico_rtos_error_entry_t *tail;
    size_t count;
    size_t max_count;
    pico_rtos_error_entry_t entries[PICO_RTOS_ERROR_HISTORY_SIZE];
} pico_rtos_error_history_t;

#endif // PICO_RTOS_ENABLE_ERROR_HISTORY

// =============================================================================
// ERROR REPORTING MACROS
// =============================================================================

/**
 * @brief Report an error with full context information
 * 
 * This macro automatically captures file, line, and function information.
 * 
 * @param code Error code to report
 * @param context_data Additional context-specific data (optional)
 */
#define PICO_RTOS_REPORT_ERROR(code, context_data) \
    pico_rtos_report_error_detailed((code), __FILE__, __LINE__, __func__, (context_data))

/**
 * @brief Report an error with simple context
 * 
 * Simplified version that only requires error code.
 * 
 * @param code Error code to report
 */
#define PICO_RTOS_REPORT_ERROR_SIMPLE(code) \
    PICO_RTOS_REPORT_ERROR((code), 0)

// =============================================================================
// PUBLIC API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize the error reporting system
 * 
 * Must be called during system initialization before any error reporting.
 * 
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_error_init(void);

/**
 * @brief Report an error with detailed context information
 * 
 * This function is typically called through the PICO_RTOS_REPORT_ERROR macro
 * to automatically capture source location information.
 * 
 * @param code Error code to report
 * @param file Source file name where error occurred
 * @param line Line number where error occurred
 * @param function Function name where error occurred
 * @param context_data Additional context-specific data
 */
void pico_rtos_report_error_detailed(pico_rtos_error_t code, 
                                   const char *file, 
                                   int line, 
                                   const char *function,
                                   uint32_t context_data);

/**
 * @brief Get human-readable description for an error code
 * 
 * @param code Error code to get description for
 * @return Pointer to static string containing error description
 */
const char *pico_rtos_get_error_description(pico_rtos_error_t code);

/**
 * @brief Get information about the last error that occurred
 * 
 * @param error_info Pointer to structure to fill with error information
 * @return true if error information was available, false if no errors occurred
 */
bool pico_rtos_get_last_error(pico_rtos_error_info_t *error_info);

/**
 * @brief Clear the last error information
 * 
 * Resets the last error to PICO_RTOS_ERROR_NONE.
 */
void pico_rtos_clear_last_error(void);

// =============================================================================
// ERROR HISTORY API (if enabled)
// =============================================================================

#if PICO_RTOS_ENABLE_ERROR_HISTORY

/**
 * @brief Get error history
 * 
 * Retrieves up to max_count error entries from the error history buffer.
 * Errors are returned in chronological order (oldest first).
 * 
 * @param errors Array to store error information
 * @param max_count Maximum number of errors to retrieve
 * @param actual_count Pointer to store actual number of errors retrieved
 * @return true if successful, false if invalid parameters
 */
bool pico_rtos_get_error_history(pico_rtos_error_info_t *errors, 
                                size_t max_count, 
                                size_t *actual_count);

/**
 * @brief Get the number of errors in history
 * 
 * @return Number of errors currently stored in history
 */
size_t pico_rtos_get_error_count(void);

/**
 * @brief Clear all error history
 * 
 * Removes all errors from the history buffer.
 */
void pico_rtos_clear_error_history(void);

/**
 * @brief Check if error history buffer is full
 * 
 * @return true if history buffer is full, false otherwise
 */
bool pico_rtos_is_error_history_full(void);

#endif // PICO_RTOS_ENABLE_ERROR_HISTORY

// =============================================================================
// ERROR STATISTICS API
// =============================================================================

/**
 * @brief Error statistics structure
 */
typedef struct {
    uint32_t total_errors;           ///< Total number of errors reported
    uint32_t task_errors;            ///< Number of task-related errors
    uint32_t memory_errors;          ///< Number of memory-related errors
    uint32_t sync_errors;            ///< Number of synchronization errors
    uint32_t system_errors;          ///< Number of system errors
    uint32_t hardware_errors;        ///< Number of hardware errors
    uint32_t config_errors;          ///< Number of configuration errors
    pico_rtos_error_t most_recent_error;  ///< Most recently reported error
    uint32_t most_recent_timestamp;  ///< Timestamp of most recent error
} pico_rtos_error_stats_t;

/**
 * @brief Get error statistics
 * 
 * @param stats Pointer to structure to fill with error statistics
 */
void pico_rtos_get_error_stats(pico_rtos_error_stats_t *stats);

/**
 * @brief Reset error statistics
 * 
 * Clears all error counters and statistics.
 */
void pico_rtos_reset_error_stats(void);

// =============================================================================
// ERROR CALLBACK SUPPORT
// =============================================================================

/**
 * @brief Error callback function type
 * 
 * User-defined callback function that gets called when an error occurs.
 * This allows applications to implement custom error handling.
 * 
 * @param error_info Pointer to error information structure
 */
typedef void (*pico_rtos_error_callback_t)(const pico_rtos_error_info_t *error_info);

/**
 * @brief Set error callback function
 * 
 * Registers a callback function to be called whenever an error is reported.
 * Only one callback can be registered at a time.
 * 
 * @param callback Pointer to callback function, or NULL to disable callbacks
 */
void pico_rtos_set_error_callback(pico_rtos_error_callback_t callback);

/**
 * @brief Get current error callback function
 * 
 * @return Pointer to current callback function, or NULL if none set
 */
pico_rtos_error_callback_t pico_rtos_get_error_callback(void);

// =============================================================================
// ASSERTION SYSTEM
// =============================================================================

/**
 * @brief Enable enhanced assertion handling
 * 
 * When disabled, assertions compile to standard assert() behavior.
 */
#ifndef PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS
#define PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS 1
#endif

/**
 * @brief Enable configurable assertion handlers
 * 
 * When enabled, allows custom assertion handlers to be registered.
 */
#ifndef PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
#define PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE 0
#endif

#if PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS

/**
 * @brief Assertion failure actions
 */
typedef enum {
    PICO_RTOS_ASSERT_ACTION_HALT,        ///< Halt system execution
    PICO_RTOS_ASSERT_ACTION_CONTINUE,    ///< Continue execution after logging
    PICO_RTOS_ASSERT_ACTION_CALLBACK,    ///< Call user-defined callback
    PICO_RTOS_ASSERT_ACTION_RESTART      ///< Restart the system
} pico_rtos_assert_action_t;

/**
 * @brief Assertion context information
 */
typedef struct {
    const char *expression;          ///< Failed assertion expression
    const char *file;                ///< Source file where assertion failed
    int line;                        ///< Line number where assertion failed
    const char *function;            ///< Function name where assertion failed
    uint32_t timestamp;              ///< System tick when assertion failed
    uint32_t task_id;                ///< ID of task where assertion failed
    const char *message;             ///< Optional custom message
} pico_rtos_assert_info_t;

/**
 * @brief Assertion statistics
 */
typedef struct {
    uint32_t total_assertions;       ///< Total number of assertions checked
    uint32_t failed_assertions;      ///< Number of failed assertions
    uint32_t continued_assertions;   ///< Number of assertions that continued execution
    uint32_t halted_assertions;      ///< Number of assertions that halted system
    pico_rtos_assert_info_t last_failure; ///< Information about last failed assertion
} pico_rtos_assert_stats_t;

#if PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE

/**
 * @brief Assertion handler function type
 * 
 * User-defined function that gets called when an assertion fails.
 * The handler can decide what action to take based on the assertion context.
 * 
 * @param assert_info Pointer to assertion context information
 * @return Action to take (halt, continue, etc.)
 */
typedef pico_rtos_assert_action_t (*pico_rtos_assert_handler_t)(const pico_rtos_assert_info_t *assert_info);

/**
 * @brief Set custom assertion handler
 * 
 * Registers a custom assertion handler function. When an assertion fails,
 * this handler will be called to determine what action to take.
 * 
 * @param handler Pointer to handler function, or NULL to use default behavior
 */
void pico_rtos_set_assert_handler(pico_rtos_assert_handler_t handler);

/**
 * @brief Get current assertion handler
 * 
 * @return Pointer to current assertion handler, or NULL if using default
 */
pico_rtos_assert_handler_t pico_rtos_get_assert_handler(void);

#endif // PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE

/**
 * @brief Set default assertion action
 * 
 * Sets the default action to take when an assertion fails.
 * 
 * @param action Default action to take
 */
void pico_rtos_set_assert_action(pico_rtos_assert_action_t action);

/**
 * @brief Get current default assertion action
 * 
 * @return Current default assertion action
 */
pico_rtos_assert_action_t pico_rtos_get_assert_action(void);

/**
 * @brief Handle assertion failure
 * 
 * Internal function called when an assertion fails. This function
 * captures context information and takes appropriate action.
 * 
 * @param expression Failed assertion expression
 * @param file Source file where assertion failed
 * @param line Line number where assertion failed
 * @param function Function name where assertion failed
 * @param message Optional custom message
 */
void pico_rtos_handle_assert_failure(const char *expression,
                                   const char *file,
                                   int line,
                                   const char *function,
                                   const char *message);

/**
 * @brief Get assertion statistics
 * 
 * @param stats Pointer to structure to fill with assertion statistics
 */
void pico_rtos_get_assert_stats(pico_rtos_assert_stats_t *stats);

/**
 * @brief Reset assertion statistics
 * 
 * Clears all assertion counters and statistics.
 */
void pico_rtos_reset_assert_stats(void);

// =============================================================================
// ASSERTION MACROS
// =============================================================================

/**
 * @brief Enhanced assertion macro with detailed context capture
 * 
 * This macro provides enhanced assertion functionality with detailed
 * context information and configurable failure handling.
 * 
 * @param expr Expression to evaluate
 */
#define PICO_RTOS_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            pico_rtos_handle_assert_failure(#expr, __FILE__, __LINE__, __func__, NULL); \
        } \
    } while(0)

/**
 * @brief Enhanced assertion macro with custom message
 * 
 * @param expr Expression to evaluate
 * @param msg Custom message to include with assertion failure
 */
#define PICO_RTOS_ASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            pico_rtos_handle_assert_failure(#expr, __FILE__, __LINE__, __func__, msg); \
        } \
    } while(0)

/**
 * @brief Assertion that always fails with a message
 * 
 * @param msg Message to display
 */
#define PICO_RTOS_ASSERT_FAIL(msg) \
    pico_rtos_handle_assert_failure("ASSERT_FAIL", __FILE__, __LINE__, __func__, msg)

/**
 * @brief Compile-time assertion
 * 
 * @param expr Expression to evaluate at compile time
 * @param msg Message to display if assertion fails
 */
#define PICO_RTOS_STATIC_ASSERT(expr, msg) \
    _Static_assert(expr, msg)

/**
 * @brief Parameter validation assertion
 * 
 * Used for validating function parameters. Can be disabled in release builds.
 * 
 * @param expr Expression to evaluate
 */
#ifndef NDEBUG
#define PICO_RTOS_ASSERT_PARAM(expr) PICO_RTOS_ASSERT(expr)
#else
#define PICO_RTOS_ASSERT_PARAM(expr) ((void)0)
#endif

/**
 * @brief Internal consistency assertion
 * 
 * Used for checking internal system state. Always enabled.
 * 
 * @param expr Expression to evaluate
 */
#define PICO_RTOS_ASSERT_INTERNAL(expr) PICO_RTOS_ASSERT(expr)

#else // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS

// When enhanced assertions are disabled, fall back to standard assert
#include <assert.h>

#define PICO_RTOS_ASSERT(expr) assert(expr)
#define PICO_RTOS_ASSERT_MSG(expr, msg) assert(expr)
#define PICO_RTOS_ASSERT_FAIL(msg) assert(0)
#define PICO_RTOS_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#define PICO_RTOS_ASSERT_PARAM(expr) assert(expr)
#define PICO_RTOS_ASSERT_INTERNAL(expr) assert(expr)

// Stub functions for disabled assertions
#define pico_rtos_set_assert_action(action) ((void)0)
#define pico_rtos_get_assert_action() (PICO_RTOS_ASSERT_ACTION_HALT)
#define pico_rtos_get_assert_stats(stats) ((void)0)
#define pico_rtos_reset_assert_stats() ((void)0)

#if PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE
#define pico_rtos_set_assert_handler(handler) ((void)0)
#define pico_rtos_get_assert_handler() (NULL)
#endif

#endif // PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS

#endif // PICO_RTOS_ERROR_H