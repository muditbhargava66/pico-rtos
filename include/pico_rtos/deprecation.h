/**
 * @file deprecation.h
 * @brief Deprecation warnings and migration support for Pico-RTOS v0.3.0
 * 
 * This header provides compile-time deprecation warnings for APIs and
 * configurations that have been superseded in v0.3.0. It helps users
 * migrate from v0.2.1 by providing clear warnings and migration paths.
 */

#ifndef PICO_RTOS_DEPRECATION_H
#define PICO_RTOS_DEPRECATION_H

#include "pico_rtos/config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Forward declarations to avoid circular dependencies
void pico_rtos_get_memory_stats(uint32_t *current, uint32_t *peak, uint32_t *allocations);

// Compiler-specific deprecation attributes
#ifdef __GNUC__
    #define PICO_RTOS_DEPRECATED(msg) __attribute__((deprecated(msg)))
    #define PICO_RTOS_DEPRECATED_ENUM(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define PICO_RTOS_DEPRECATED(msg) __declspec(deprecated(msg))
    #define PICO_RTOS_DEPRECATED_ENUM(msg) 
#else
    #define PICO_RTOS_DEPRECATED(msg)
    #define PICO_RTOS_DEPRECATED_ENUM(msg)
#endif

// Deprecation warning control
#if defined(PICO_RTOS_ENABLE_MIGRATION_WARNINGS) && !defined(PICO_RTOS_DISABLE_MIGRATION_WARNINGS)
    #define PICO_RTOS_MIGRATION_WARNING(msg) PICO_RTOS_DEPRECATED(msg)
    #define PICO_RTOS_MIGRATION_WARNING_ENUM(msg) PICO_RTOS_DEPRECATED_ENUM(msg)
#else
    #define PICO_RTOS_MIGRATION_WARNING(msg)
    #define PICO_RTOS_MIGRATION_WARNING_ENUM(msg)
#endif

// Configuration deprecation warnings
#if defined(PICO_RTOS_ENABLE_MIGRATION_WARNINGS) && !defined(PICO_RTOS_DISABLE_MIGRATION_WARNINGS)

// Check for deprecated configuration patterns
#ifdef CONFIG_OLD_TIMER_API
    #warning "CONFIG_OLD_TIMER_API is deprecated. Use CONFIG_ENABLE_HIRES_TIMERS for high-resolution timing in v0.3.0"
#endif

#ifdef CONFIG_SIMPLE_LOGGING
    #warning "CONFIG_SIMPLE_LOGGING is deprecated. Use CONFIG_ENABLE_ENHANCED_LOGGING for advanced logging features in v0.3.0"
#endif

#ifdef CONFIG_BASIC_MEMORY_TRACKING
    #warning "CONFIG_BASIC_MEMORY_TRACKING is deprecated. Memory tracking is now always available when CONFIG_ENABLE_MEMORY_TRACKING is enabled in v0.3.0"
#endif

#ifdef CONFIG_LEGACY_QUEUE_API
    #warning "CONFIG_LEGACY_QUEUE_API is deprecated. The standard queue API now includes all legacy functionality in v0.3.0"
#endif

#ifdef CONFIG_SIMPLE_SCHEDULER
    #warning "CONFIG_SIMPLE_SCHEDULER is deprecated. Use CONFIG_ENABLE_MULTI_CORE=n for single-core operation in v0.3.0"
#endif

// Check for potentially problematic configurations
#if defined(CONFIG_TICK_RATE_HZ) && (CONFIG_TICK_RATE_HZ > 2000)
    #warning "High tick rates (>2000 Hz) may impact performance. Consider using CONFIG_ENABLE_HIRES_TIMERS for precise timing in v0.3.0"
#endif

#if defined(CONFIG_MAX_TASKS) && (CONFIG_MAX_TASKS > 32)
    #warning "High task counts (>32) may impact performance. Consider task consolidation or enabling CONFIG_ENABLE_MULTI_CORE in v0.3.0"
#endif

// Check for missing recommended configurations
#if defined(PICO_RTOS_ENABLE_MULTI_CORE) && !defined(PICO_RTOS_ENABLE_MEMORY_TRACKING)
    #warning "Multi-core support works best with memory tracking enabled. Consider enabling CONFIG_ENABLE_MEMORY_TRACKING in v0.3.0"
#endif

#if defined(PICO_RTOS_ENABLE_MULTI_CORE) && !defined(PICO_RTOS_ENABLE_RUNTIME_STATS)
    #warning "Multi-core support requires runtime statistics for load balancing. Consider enabling CONFIG_ENABLE_RUNTIME_STATS in v0.3.0"
#endif

#endif // PICO_RTOS_ENABLE_MIGRATION_WARNINGS

// Deprecated API function declarations
// These provide backward compatibility while warning about deprecation

/**
 * @brief Deprecated timer API - use high-resolution timers instead
 * @deprecated Use pico_rtos_hires_timer_* functions for microsecond precision
 */
PICO_RTOS_MIGRATION_WARNING("Use pico_rtos_hires_timer_create() for high-resolution timing")
static inline bool pico_rtos_timer_create_precise(void *timer, const char *name, 
                                                 void (*callback)(void*), void *param, 
                                                 uint32_t period_us, bool auto_reload) {
    // Redirect to standard timer API with conversion
    #ifdef PICO_RTOS_ENABLE_HIRES_TIMERS
        // If high-res timers are available, suggest using them
        return false; // Force user to migrate to new API
    #else
        // Fallback to standard timer with millisecond conversion
        // Temporarily disabled due to type conflicts
        (void)timer; (void)name; (void)callback; (void)param; (void)period_us; (void)auto_reload;
        return false;
    #endif
}

/**
 * @brief Deprecated simple logging function
 * @deprecated Use pico_rtos_log() with log levels for enhanced logging
 */
PICO_RTOS_MIGRATION_WARNING("Use pico_rtos_log(level, format, ...) for enhanced logging features")
static inline void pico_rtos_simple_log(const char *message) {
    // Temporarily disabled due to type conflicts
    printf("%s\n", message);
}

/**
 * @brief Deprecated memory tracking function
 * @deprecated Use pico_rtos_get_memory_stats() for comprehensive memory information
 */
PICO_RTOS_MIGRATION_WARNING("Use pico_rtos_get_memory_stats() for detailed memory information")
static inline uint32_t pico_rtos_get_memory_usage(void) {
    #ifdef PICO_RTOS_ENABLE_MEMORY_TRACKING
        uint32_t current, peak, allocations;
        pico_rtos_get_memory_stats(&current, &peak, &allocations);
        return current;
    #else
        return 0;
    #endif
}

/**
 * @brief Deprecated queue creation with simple parameters
 * @deprecated Use pico_rtos_queue_init() with explicit buffer management
 */
PICO_RTOS_MIGRATION_WARNING("Use pico_rtos_queue_init() with explicit buffer for better memory management")
static inline bool pico_rtos_queue_create_simple(void *queue, uint32_t item_size, uint32_t max_items) {
    // This would require dynamic allocation, which we want to discourage
    return false; // Force user to provide explicit buffer
}

// Deprecated enumeration values
typedef enum {
    PICO_RTOS_TIMER_MODE_ONESHOT PICO_RTOS_MIGRATION_WARNING_ENUM("Use auto_reload=false in timer creation") = 0,
    PICO_RTOS_TIMER_MODE_PERIODIC PICO_RTOS_MIGRATION_WARNING_ENUM("Use auto_reload=true in timer creation") = 1
} pico_rtos_deprecated_timer_mode_t;

typedef enum {
    PICO_RTOS_LOG_SIMPLE PICO_RTOS_MIGRATION_WARNING_ENUM("Use PICO_RTOS_LOG_INFO for informational messages") = 0,
    PICO_RTOS_LOG_ERROR_ONLY PICO_RTOS_MIGRATION_WARNING_ENUM("Use PICO_RTOS_LOG_ERROR for error messages") = 1
} pico_rtos_deprecated_log_level_t;

// Deprecated macro definitions with warnings
#if defined(PICO_RTOS_ENABLE_MIGRATION_WARNINGS) && !defined(PICO_RTOS_DISABLE_MIGRATION_WARNINGS)

#define PICO_RTOS_WAIT_INFINITE PICO_RTOS_WAIT_FOREVER
#pragma message("PICO_RTOS_WAIT_INFINITE is deprecated. Use PICO_RTOS_WAIT_FOREVER for consistency in v0.3.0")

#define PICO_RTOS_NO_TIMEOUT PICO_RTOS_NO_WAIT
#pragma message("PICO_RTOS_NO_TIMEOUT is deprecated. Use PICO_RTOS_NO_WAIT for consistency in v0.3.0")

#define PICO_RTOS_TASK_PRIORITY_IDLE 0
#pragma message("PICO_RTOS_TASK_PRIORITY_IDLE is deprecated. Use explicit priority values (0 = lowest) in v0.3.0")

#define PICO_RTOS_TASK_PRIORITY_NORMAL 5
#pragma message("PICO_RTOS_TASK_PRIORITY_NORMAL is deprecated. Use explicit priority values based on your needs in v0.3.0")

#define PICO_RTOS_TASK_PRIORITY_HIGH 10
#pragma message("PICO_RTOS_TASK_PRIORITY_HIGH is deprecated. Use explicit priority values based on your needs in v0.3.0")

#elif defined(PICO_RTOS_ENABLE_MIGRATION_WARNINGS)
// Provide deprecated macros without warnings when explicitly disabled
#define PICO_RTOS_WAIT_INFINITE PICO_RTOS_WAIT_FOREVER
#define PICO_RTOS_NO_TIMEOUT PICO_RTOS_NO_WAIT
#define PICO_RTOS_TASK_PRIORITY_IDLE 0
#define PICO_RTOS_TASK_PRIORITY_NORMAL 5
#define PICO_RTOS_TASK_PRIORITY_HIGH 10

#endif // PICO_RTOS_ENABLE_MIGRATION_WARNINGS

// Migration helper macros
#define PICO_RTOS_MIGRATION_GUIDE_URL "https://github.com/your-repo/pico-rtos/blob/main/docs/migration_guide.md"

#if defined(PICO_RTOS_ENABLE_MIGRATION_WARNINGS) && !defined(PICO_RTOS_DISABLE_MIGRATION_WARNINGS)
    #define PICO_RTOS_MIGRATION_NOTICE(feature, replacement) \
        _Pragma("message(\"Feature has been enhanced in v0.3.0. See migration guide.\")")
#else
    #define PICO_RTOS_MIGRATION_NOTICE(feature, replacement)
#endif

// Feature availability checks with migration suggestions
#ifndef PICO_RTOS_ENABLE_EVENT_GROUPS
    PICO_RTOS_MIGRATION_NOTICE("Event synchronization", "CONFIG_ENABLE_EVENT_GROUPS for advanced multi-event coordination")
#endif

#ifndef PICO_RTOS_ENABLE_STREAM_BUFFERS
    PICO_RTOS_MIGRATION_NOTICE("Variable-length messaging", "CONFIG_ENABLE_STREAM_BUFFERS for efficient data streaming")
#endif

#ifndef PICO_RTOS_ENABLE_MEMORY_POOLS
    PICO_RTOS_MIGRATION_NOTICE("Fixed-size allocation", "CONFIG_ENABLE_MEMORY_POOLS for O(1) deterministic allocation")
#endif

#ifndef PICO_RTOS_ENABLE_MULTI_CORE
    PICO_RTOS_MIGRATION_NOTICE("Single-core operation", "CONFIG_ENABLE_MULTI_CORE for dual-core RP2040 performance")
#endif

#ifndef PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING
    PICO_RTOS_MIGRATION_NOTICE("System monitoring", "CONFIG_ENABLE_SYSTEM_HEALTH_MONITORING for production diagnostics")
#endif

// Deprecation timeline information
typedef struct {
    const char *feature;
    const char *deprecated_version;
    const char *removal_version;
    const char *replacement;
    const char *migration_notes;
} pico_rtos_deprecation_info_t;

// Deprecation schedule (for documentation and tooling)
static const pico_rtos_deprecation_info_t pico_rtos_deprecation_schedule[] = {
    {
        .feature = "pico_rtos_timer_create_precise",
        .deprecated_version = "0.3.0",
        .removal_version = "0.4.0",
        .replacement = "pico_rtos_hires_timer_create",
        .migration_notes = "High-resolution timers provide microsecond precision"
    },
    {
        .feature = "pico_rtos_simple_log",
        .deprecated_version = "0.3.0", 
        .removal_version = "0.4.0",
        .replacement = "pico_rtos_log with log levels",
        .migration_notes = "Enhanced logging provides better control and filtering"
    },
    {
        .feature = "pico_rtos_get_memory_usage",
        .deprecated_version = "0.3.0",
        .removal_version = "0.4.0", 
        .replacement = "pico_rtos_get_memory_stats",
        .migration_notes = "Comprehensive memory statistics provide more information"
    },
    {
        .feature = "PICO_RTOS_WAIT_INFINITE",
        .deprecated_version = "0.3.0",
        .removal_version = "0.4.0",
        .replacement = "PICO_RTOS_WAIT_FOREVER",
        .migration_notes = "Consistent naming across the API"
    },
    {
        .feature = "CONFIG_OLD_TIMER_API",
        .deprecated_version = "0.3.0",
        .removal_version = "0.4.0",
        .replacement = "CONFIG_ENABLE_HIRES_TIMERS",
        .migration_notes = "New timer system provides better precision and features"
    }
};

#define PICO_RTOS_DEPRECATION_COUNT (sizeof(pico_rtos_deprecation_schedule) / sizeof(pico_rtos_deprecation_schedule[0]))

/**
 * @brief Get deprecation information for a feature
 * @param feature_name Name of the deprecated feature
 * @return Pointer to deprecation info, or NULL if not found
 */
const pico_rtos_deprecation_info_t *pico_rtos_get_deprecation_info(const char *feature_name);

/**
 * @brief Print all deprecation warnings for currently used deprecated features
 * This function can be called during initialization to show all active deprecations
 */
void pico_rtos_print_deprecation_warnings(void);

/**
 * @brief Check if a feature is deprecated
 * @param feature_name Name of the feature to check
 * @return true if deprecated, false otherwise
 */
bool pico_rtos_is_feature_deprecated(const char *feature_name);

/**
 * @brief Initialize the deprecation warning system
 * Call this during RTOS initialization to show deprecation warnings
 */
void pico_rtos_deprecation_init(void);

#endif // PICO_RTOS_DEPRECATION_H