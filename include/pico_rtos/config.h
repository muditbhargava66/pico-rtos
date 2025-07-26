#ifndef PICO_RTOS_CONFIG_H
#define PICO_RTOS_CONFIG_H

/**
 * @file config.h
 * @brief Pico-RTOS Configuration Options
 * 
 * This file contains compile-time configuration options for Pico-RTOS.
 * These options can be overridden by defining them before including
 * this header or through CMake configuration.
 */

// =============================================================================
// SYSTEM TICK FREQUENCY CONFIGURATION
// =============================================================================

/**
 * @brief Predefined system tick frequency options (Hz)
 * 
 * These are common tick frequencies that provide good balance between
 * timing resolution and system overhead.
 */
#define PICO_RTOS_TICK_RATE_HZ_100    100   ///< 100 Hz (10ms tick period)
#define PICO_RTOS_TICK_RATE_HZ_250    250   ///< 250 Hz (4ms tick period)
#define PICO_RTOS_TICK_RATE_HZ_500    500   ///< 500 Hz (2ms tick period)
#define PICO_RTOS_TICK_RATE_HZ_1000   1000  ///< 1000 Hz (1ms tick period) - Default
#define PICO_RTOS_TICK_RATE_HZ_2000   2000  ///< 2000 Hz (0.5ms tick period)

/**
 * @brief System tick frequency in Hz
 * 
 * This defines how often the system tick interrupt occurs. Higher frequencies
 * provide better timing resolution but increase system overhead.
 * 
 * Valid range: 10 Hz to 10000 Hz
 * Recommended values: Use predefined constants above
 * 
 * @note This can be overridden via CMake: -DPICO_RTOS_TICK_RATE_HZ=500
 */
#ifndef PICO_RTOS_TICK_RATE_HZ
#define PICO_RTOS_TICK_RATE_HZ PICO_RTOS_TICK_RATE_HZ_1000
#endif

// Compile-time validation of tick frequency
#if PICO_RTOS_TICK_RATE_HZ < 10
#error "PICO_RTOS_TICK_RATE_HZ must be at least 10 Hz"
#endif

#if PICO_RTOS_TICK_RATE_HZ > 10000
#error "PICO_RTOS_TICK_RATE_HZ must not exceed 10000 Hz"
#endif

/**
 * @brief Calculate tick period in microseconds
 * 
 * This is used internally for timer calculations.
 */
#define PICO_RTOS_TICK_PERIOD_US (1000000UL / PICO_RTOS_TICK_RATE_HZ)

// =============================================================================
// SYSTEM LIMITS AND SIZING
// =============================================================================

/**
 * @brief Maximum number of tasks supported by the system
 * 
 * This affects memory usage for task management structures.
 * Can be overridden via CMake: -DPICO_RTOS_MAX_TASKS=32
 */
#ifndef PICO_RTOS_MAX_TASKS
#define PICO_RTOS_MAX_TASKS 16
#endif

/**
 * @brief Maximum number of software timers supported
 * 
 * This affects memory usage for timer management structures.
 * Can be overridden via CMake: -DPICO_RTOS_MAX_TIMERS=16
 */
#ifndef PICO_RTOS_MAX_TIMERS
#define PICO_RTOS_MAX_TIMERS 8
#endif

// =============================================================================
// FEATURE ENABLE/DISABLE FLAGS
// =============================================================================

/**
 * @brief Enable stack overflow checking
 * 
 * When enabled, the system will check for stack overflows periodically
 * and call the stack overflow handler if detected.
 */
#ifndef PICO_RTOS_ENABLE_STACK_CHECKING
#define PICO_RTOS_ENABLE_STACK_CHECKING 1
#endif

/**
 * @brief Enable memory usage tracking
 * 
 * When enabled, the system tracks memory allocations and provides
 * statistics through pico_rtos_get_memory_stats().
 */
#ifndef PICO_RTOS_ENABLE_MEMORY_TRACKING
#define PICO_RTOS_ENABLE_MEMORY_TRACKING 1
#endif

/**
 * @brief Enable runtime statistics collection
 * 
 * When enabled, the system collects runtime statistics such as
 * task execution times and system load.
 */
#ifndef PICO_RTOS_ENABLE_RUNTIME_STATS
#define PICO_RTOS_ENABLE_RUNTIME_STATS 1
#endif

// =============================================================================
// DEBUG AND LOGGING CONFIGURATION
// =============================================================================

/**
 * @brief Enable debug logging system
 * 
 * When enabled, the system provides configurable debug logging.
 * When disabled, all logging macros compile to nothing (zero overhead).
 */
#ifndef PICO_RTOS_ENABLE_LOGGING
#define PICO_RTOS_ENABLE_LOGGING 0
#endif

/**
 * @brief Enable error history tracking
 * 
 * When enabled, the system maintains a history of errors for debugging.
 */
#ifndef PICO_RTOS_ENABLE_ERROR_HISTORY
#define PICO_RTOS_ENABLE_ERROR_HISTORY 1
#endif

// =============================================================================
// MULTI-CORE (SMP) CONFIGURATION
// =============================================================================

/**
 * @brief Enable multi-core support
 * 
 * When enabled, the system includes multi-core specific fields in task structures
 * and provides SMP functionality. When disabled, single-core mode with zero overhead.
 */
#ifndef PICO_RTOS_ENABLE_MULTI_CORE
#define PICO_RTOS_ENABLE_MULTI_CORE 1
#endif

/**
 * @brief Enable Symmetric Multi-Processing (SMP) support
 * 
 * When enabled, the system supports task scheduling across both RP2040 cores.
 * When disabled, the system operates in single-core mode with zero overhead.
 */
#ifndef PICO_RTOS_ENABLE_SMP
#define PICO_RTOS_ENABLE_SMP PICO_RTOS_ENABLE_MULTI_CORE
#endif

/**
 * @brief Maximum tasks per core in SMP mode
 * 
 * This limits the number of tasks that can be assigned to each core.
 * Total system tasks = PICO_RTOS_MAX_TASKS_PER_CORE * 2
 */
#ifndef PICO_RTOS_MAX_TASKS_PER_CORE
#define PICO_RTOS_MAX_TASKS_PER_CORE 8
#endif

/**
 * @brief Load balancing threshold percentage
 * 
 * When the load difference between cores exceeds this threshold,
 * the load balancer will attempt to migrate tasks.
 */
#ifndef PICO_RTOS_SMP_LOAD_BALANCE_THRESHOLD
#define PICO_RTOS_SMP_LOAD_BALANCE_THRESHOLD 25
#endif

/**
 * @brief Enable SMP load balancing
 * 
 * When enabled, the system automatically balances task load between cores.
 * When disabled, tasks stay on their initially assigned cores.
 */
#ifndef PICO_RTOS_ENABLE_SMP_LOAD_BALANCING
#define PICO_RTOS_ENABLE_SMP_LOAD_BALANCING 1
#endif

/**
 * @brief Task-local storage slots per task
 * 
 * Number of task-local storage slots available for each task in SMP mode.
 */
#ifndef PICO_RTOS_SMP_TLS_SLOTS
#define PICO_RTOS_SMP_TLS_SLOTS 4
#endif

/**
 * @brief Size of error history buffer
 * 
 * Number of error entries to keep in the error history.
 */
#ifndef PICO_RTOS_ERROR_HISTORY_SIZE
#define PICO_RTOS_ERROR_HISTORY_SIZE 10
#endif

// =============================================================================
// PERFORMANCE TUNING
// =============================================================================

/**
 * @brief Idle task stack size in words (4 bytes each)
 * 
 * Stack size for the idle task. Should be sufficient for idle hook functions.
 */
#ifndef PICO_RTOS_IDLE_TASK_STACK_SIZE
#define PICO_RTOS_IDLE_TASK_STACK_SIZE 64
#endif

/**
 * @brief Default task stack size in bytes
 * 
 * Default stack size used when creating tasks without specifying stack size.
 */
#ifndef PICO_RTOS_DEFAULT_TASK_STACK_SIZE
#define PICO_RTOS_DEFAULT_TASK_STACK_SIZE 1024
#endif

// =============================================================================
// VALIDATION MACROS
// =============================================================================

// Validate configuration dependencies
#if PICO_RTOS_ENABLE_ERROR_HISTORY && !PICO_RTOS_ENABLE_MEMORY_TRACKING
#warning "Error history requires memory tracking to be enabled for optimal operation"
#endif

#if PICO_RTOS_MAX_TASKS < 1
#error "PICO_RTOS_MAX_TASKS must be at least 1"
#endif

#if PICO_RTOS_MAX_TIMERS < 1
#error "PICO_RTOS_MAX_TIMERS must be at least 1"
#endif

#if PICO_RTOS_ERROR_HISTORY_SIZE < 1
#error "PICO_RTOS_ERROR_HISTORY_SIZE must be at least 1"
#endif

#if PICO_RTOS_IDLE_TASK_STACK_SIZE < 32
#error "PICO_RTOS_IDLE_TASK_STACK_SIZE must be at least 32 words (128 bytes)"
#endif

#if PICO_RTOS_DEFAULT_TASK_STACK_SIZE < 256
#error "PICO_RTOS_DEFAULT_TASK_STACK_SIZE must be at least 256 bytes"
#endif

#endif // PICO_RTOS_CONFIG_H