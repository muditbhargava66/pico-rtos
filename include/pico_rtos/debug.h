#ifndef PICO_RTOS_DEBUG_H
#define PICO_RTOS_DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

/**
 * @file debug.h
 * @brief Pico-RTOS Runtime Task Inspection and Debugging System
 * 
 * This module provides non-intrusive task state inspection capabilities
 * for debugging and monitoring without affecting real-time behavior.
 */

// Forward declaration
typedef struct pico_rtos_task pico_rtos_task_t;

// =============================================================================
// CONFIGURATION
// =============================================================================

/**
 * @brief Enable debug features
 * 
 * When disabled, all debug functions compile to empty stubs with zero overhead.
 */
#ifndef PICO_RTOS_ENABLE_DEBUG
#define PICO_RTOS_ENABLE_DEBUG 1
#endif

/**
 * @brief Maximum number of tasks that can be inspected simultaneously
 */
#ifndef PICO_RTOS_DEBUG_MAX_TASKS
#define PICO_RTOS_DEBUG_MAX_TASKS 32
#endif

/**
 * @brief Stack usage calculation method
 */
typedef enum {
    PICO_RTOS_STACK_USAGE_PATTERN,    ///< Use stack pattern checking (more accurate)
    PICO_RTOS_STACK_USAGE_WATERMARK   ///< Use high-water mark tracking (faster)
} pico_rtos_stack_usage_method_t;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Task information structure for inspection
 * 
 * Contains comprehensive information about a task's current state
 * and runtime statistics.
 */
typedef struct {
    const char *name;                      ///< Task name
    uint32_t priority;                     ///< Current priority
    uint32_t original_priority;            ///< Original priority (before inheritance)
    pico_rtos_task_state_t state;          ///< Current task state
    pico_rtos_block_reason_t block_reason; ///< Reason for blocking (if blocked)
    
    // Stack information
    uint32_t stack_size;                   ///< Total stack size in bytes
    uint32_t stack_usage;                  ///< Current stack usage in bytes
    uint32_t stack_high_water;             ///< Peak stack usage in bytes
    uint32_t stack_free;                   ///< Free stack space in bytes
    float stack_usage_percent;             ///< Stack usage as percentage
    
    // Runtime statistics
    uint64_t cpu_time_us;                  ///< Total CPU time in microseconds
    uint32_t context_switches;             ///< Number of context switches
    uint32_t last_run_time;                ///< Last execution timestamp
    uint32_t creation_time;                ///< Task creation timestamp
    
    // Memory information
    void *stack_base;                      ///< Stack base address
    void *stack_ptr;                       ///< Current stack pointer
    void *stack_top;                       ///< Stack top address
    
    // Task function information
    void *function_ptr;                    ///< Task function pointer
    void *param;                           ///< Task parameter
    
    // Blocking information
    void *blocking_object;                 ///< Object task is blocked on (if any)
    uint32_t block_start_time;             ///< When blocking started
    uint32_t block_timeout;                ///< Blocking timeout value
} pico_rtos_task_info_t;

/**
 * @brief System-wide task inspection summary
 */
typedef struct {
    uint32_t total_tasks;                  ///< Total number of tasks
    uint32_t ready_tasks;                  ///< Tasks in ready state
    uint32_t running_tasks;                ///< Tasks in running state
    uint32_t blocked_tasks;                ///< Tasks in blocked state
    uint32_t suspended_tasks;              ///< Tasks in suspended state
    uint32_t terminated_tasks;             ///< Tasks in terminated state
    
    uint32_t total_stack_allocated;        ///< Total stack memory allocated
    uint32_t total_stack_used;             ///< Total stack memory used
    uint32_t highest_stack_usage_percent;  ///< Highest stack usage percentage
    
    uint32_t total_context_switches;       ///< Total context switches
    uint64_t total_cpu_time_us;            ///< Total CPU time across all tasks
    
    uint32_t inspection_time_us;           ///< Time taken for this inspection
    uint32_t inspection_timestamp;         ///< When inspection was performed
} pico_rtos_system_inspection_t;

/**
 * @brief Stack overflow detection information
 */
typedef struct {
    pico_rtos_task_t *task;                ///< Task with stack overflow
    const char *task_name;                 ///< Task name
    uint32_t stack_size;                   ///< Stack size
    uint32_t overflow_bytes;               ///< Bytes overflowed
    void *stack_base;                      ///< Stack base address
    void *current_sp;                      ///< Current stack pointer
    uint32_t detection_time;               ///< When overflow was detected
} pico_rtos_stack_overflow_info_t;

// =============================================================================
// TASK INSPECTION API
// =============================================================================

#if PICO_RTOS_ENABLE_DEBUG

/**
 * @brief Get detailed information about a specific task
 * 
 * This function provides comprehensive information about a task's current
 * state, stack usage, and runtime statistics without affecting the task's
 * execution.
 * 
 * @param task Task to inspect (NULL for current task)
 * @param info Pointer to structure to fill with task information
 * @return true if inspection was successful, false otherwise
 */
bool pico_rtos_debug_get_task_info(pico_rtos_task_t *task, pico_rtos_task_info_t *info);

/**
 * @brief Get information about all tasks in the system
 * 
 * Fills an array with information about all tasks currently in the system.
 * The inspection is performed atomically to ensure consistency.
 * 
 * @param info_array Array to fill with task information
 * @param max_tasks Maximum number of tasks to inspect
 * @return Number of tasks actually inspected
 */
uint32_t pico_rtos_debug_get_all_task_info(pico_rtos_task_info_t *info_array, uint32_t max_tasks);

/**
 * @brief Get system-wide task inspection summary
 * 
 * Provides a high-level overview of all tasks in the system including
 * aggregate statistics and resource usage.
 * 
 * @param summary Pointer to structure to fill with summary information
 * @return true if inspection was successful, false otherwise
 */
bool pico_rtos_debug_get_system_inspection(pico_rtos_system_inspection_t *summary);

/**
 * @brief Find a task by name
 * 
 * Searches for a task with the specified name and returns its information.
 * 
 * @param name Task name to search for
 * @param info Pointer to structure to fill with task information
 * @return true if task was found, false otherwise
 */
bool pico_rtos_debug_find_task_by_name(const char *name, pico_rtos_task_info_t *info);

// =============================================================================
// STACK MONITORING API
// =============================================================================

/**
 * @brief Set stack usage calculation method
 * 
 * Configures how stack usage is calculated. Pattern method is more accurate
 * but slower, watermark method is faster but less precise.
 * 
 * @param method Stack usage calculation method
 */
void pico_rtos_debug_set_stack_usage_method(pico_rtos_stack_usage_method_t method);

/**
 * @brief Get current stack usage calculation method
 * 
 * @return Current stack usage calculation method
 */
pico_rtos_stack_usage_method_t pico_rtos_debug_get_stack_usage_method(void);

/**
 * @brief Calculate stack usage for a specific task
 * 
 * Calculates the current and peak stack usage for the specified task.
 * This function uses the configured stack usage method.
 * 
 * @param task Task to calculate stack usage for (NULL for current task)
 * @param current_usage Pointer to store current stack usage (bytes)
 * @param peak_usage Pointer to store peak stack usage (bytes)
 * @param free_space Pointer to store free stack space (bytes)
 * @return true if calculation was successful, false otherwise
 */
bool pico_rtos_debug_calculate_stack_usage(pico_rtos_task_t *task, 
                                          uint32_t *current_usage,
                                          uint32_t *peak_usage, 
                                          uint32_t *free_space);

/**
 * @brief Check for stack overflow in a specific task
 * 
 * Checks if the specified task has experienced a stack overflow.
 * 
 * @param task Task to check (NULL for current task)
 * @param overflow_info Pointer to structure to fill with overflow information (optional)
 * @return true if stack overflow detected, false otherwise
 */
bool pico_rtos_debug_check_stack_overflow(pico_rtos_task_t *task, 
                                         pico_rtos_stack_overflow_info_t *overflow_info);

/**
 * @brief Check for stack overflow in all tasks
 * 
 * Performs stack overflow checking on all tasks in the system.
 * 
 * @param overflow_info Array to store overflow information for affected tasks
 * @param max_overflows Maximum number of overflows to report
 * @return Number of tasks with stack overflow detected
 */
uint32_t pico_rtos_debug_check_all_stack_overflows(pico_rtos_stack_overflow_info_t *overflow_info,
                                                   uint32_t max_overflows);

/**
 * @brief Initialize stack pattern for a task
 * 
 * Fills the task's stack with a known pattern for usage calculation.
 * This should be called during task creation if pattern-based stack
 * usage calculation is desired.
 * 
 * @param task Task to initialize stack pattern for
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_debug_init_stack_pattern(pico_rtos_task_t *task);

// =============================================================================
// RUNTIME STATISTICS API
// =============================================================================

/**
 * @brief Reset runtime statistics for a specific task
 * 
 * Resets CPU time, context switch count, and other runtime statistics
 * for the specified task.
 * 
 * @param task Task to reset statistics for (NULL for current task)
 * @return true if reset was successful, false otherwise
 */
bool pico_rtos_debug_reset_task_stats(pico_rtos_task_t *task);

/**
 * @brief Reset runtime statistics for all tasks
 * 
 * Resets runtime statistics for all tasks in the system.
 */
void pico_rtos_debug_reset_all_task_stats(void);

/**
 * @brief Get CPU usage percentage for a task
 * 
 * Calculates the CPU usage percentage for a task over a specified period.
 * 
 * @param task Task to calculate CPU usage for (NULL for current task)
 * @param period_ms Period over which to calculate usage (0 for since last reset)
 * @return CPU usage percentage (0.0 to 100.0)
 */
float pico_rtos_debug_get_task_cpu_usage(pico_rtos_task_t *task, uint32_t period_ms);

/**
 * @brief Get system-wide CPU usage statistics
 * 
 * Calculates CPU usage for all tasks and idle time.
 * 
 * @param task_usage Array to store per-task CPU usage percentages
 * @param max_tasks Maximum number of tasks to report
 * @param idle_percentage Pointer to store idle CPU percentage
 * @return Number of tasks reported
 */
uint32_t pico_rtos_debug_get_system_cpu_usage(float *task_usage, uint32_t max_tasks, 
                                              float *idle_percentage);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Convert task state to human-readable string
 * 
 * @param state Task state to convert
 * @return String representation of the task state
 */
const char *pico_rtos_debug_task_state_to_string(pico_rtos_task_state_t state);

/**
 * @brief Convert block reason to human-readable string
 * 
 * @param reason Block reason to convert
 * @return String representation of the block reason
 */
const char *pico_rtos_debug_block_reason_to_string(pico_rtos_block_reason_t reason);

/**
 * @brief Format task information as string
 * 
 * Formats task information into a human-readable string for debugging output.
 * 
 * @param info Task information to format
 * @param buffer Buffer to store formatted string
 * @param buffer_size Size of the buffer
 * @return Number of characters written (excluding null terminator)
 */
uint32_t pico_rtos_debug_format_task_info(const pico_rtos_task_info_t *info, 
                                         char *buffer, uint32_t buffer_size);

/**
 * @brief Print task information to console
 * 
 * Prints formatted task information to the console for debugging.
 * 
 * @param info Task information to print
 */
void pico_rtos_debug_print_task_info(const pico_rtos_task_info_t *info);

/**
 * @brief Print system inspection summary to console
 * 
 * Prints formatted system inspection summary to the console.
 * 
 * @param summary System inspection summary to print
 */
void pico_rtos_debug_print_system_inspection(const pico_rtos_system_inspection_t *summary);

#else // PICO_RTOS_ENABLE_DEBUG

// When debug is disabled, all functions become empty stubs
#define pico_rtos_debug_get_task_info(task, info) (false)
#define pico_rtos_debug_get_all_task_info(info_array, max_tasks) (0)
#define pico_rtos_debug_get_system_inspection(summary) (false)
#define pico_rtos_debug_find_task_by_name(name, info) (false)
#define pico_rtos_debug_set_stack_usage_method(method) ((void)0)
#define pico_rtos_debug_get_stack_usage_method() (PICO_RTOS_STACK_USAGE_WATERMARK)
#define pico_rtos_debug_calculate_stack_usage(task, current, peak, free) (false)
#define pico_rtos_debug_check_stack_overflow(task, info) (false)
#define pico_rtos_debug_check_all_stack_overflows(info, max) (0)
#define pico_rtos_debug_init_stack_pattern(task) (false)
#define pico_rtos_debug_reset_task_stats(task) (false)
#define pico_rtos_debug_reset_all_task_stats() ((void)0)
#define pico_rtos_debug_get_task_cpu_usage(task, period) (0.0f)
#define pico_rtos_debug_get_system_cpu_usage(usage, max, idle) (0)
#define pico_rtos_debug_task_state_to_string(state) ("DISABLED")
#define pico_rtos_debug_block_reason_to_string(reason) ("DISABLED")
#define pico_rtos_debug_format_task_info(info, buffer, size) (0)
#define pico_rtos_debug_print_task_info(info) ((void)0)
#define pico_rtos_debug_print_system_inspection(summary) ((void)0)

#endif // PICO_RTOS_ENABLE_DEBUG

#endif // PICO_RTOS_DEBUG_H