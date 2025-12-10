/**
 * @file deprecation.c
 * @brief Implementation of deprecation warning system for Pico-RTOS v0.3.1
 */

#include "pico_rtos/deprecation.h"
#include "pico_rtos/logging.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief Get deprecation information for a feature
 */
const pico_rtos_deprecation_info_t *pico_rtos_get_deprecation_info(const char *feature_name) {
    if (!feature_name) {
        return NULL;
    }
    
    for (size_t i = 0; i < PICO_RTOS_DEPRECATION_COUNT; i++) {
        if (strcmp(pico_rtos_deprecation_schedule[i].feature, feature_name) == 0) {
            return &pico_rtos_deprecation_schedule[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Check if a feature is deprecated
 */
bool pico_rtos_is_feature_deprecated(const char *feature_name) {
    return pico_rtos_get_deprecation_info(feature_name) != NULL;
}

/**
 * @brief Print deprecation warning for a specific feature
 */
static void print_deprecation_warning(const pico_rtos_deprecation_info_t *info) {
    if (!info) return;
    
    printf("DEPRECATION WARNING: %s\n", info->feature);
    printf("  Deprecated in: v%s\n", info->deprecated_version);
    printf("  Will be removed in: v%s\n", info->removal_version);
    printf("  Replacement: %s\n", info->replacement);
    if (info->migration_notes) {
        printf("  Migration notes: %s\n", info->migration_notes);
    }
    printf("  See migration guide: %s\n", PICO_RTOS_MIGRATION_GUIDE_URL);
    printf("\n");
}

/**
 * @brief Print all deprecation warnings for currently used deprecated features
 */
void pico_rtos_print_deprecation_warnings(void) {
    bool found_deprecated = false;
    
    printf("=== Pico-RTOS v0.3.1 Deprecation Warnings ===\n\n");
    
    // Check for deprecated configuration options
    #ifdef CONFIG_OLD_TIMER_API
        printf("DEPRECATED CONFIG: CONFIG_OLD_TIMER_API\n");
        printf("  Use CONFIG_ENABLE_HIRES_TIMERS for high-resolution timing\n\n");
        found_deprecated = true;
    #endif
    
    #ifdef CONFIG_SIMPLE_LOGGING
        printf("DEPRECATED CONFIG: CONFIG_SIMPLE_LOGGING\n");
        printf("  Use CONFIG_ENABLE_ENHANCED_LOGGING for advanced logging features\n\n");
        found_deprecated = true;
    #endif
    
    #ifdef CONFIG_BASIC_MEMORY_TRACKING
        printf("DEPRECATED CONFIG: CONFIG_BASIC_MEMORY_TRACKING\n");
        printf("  Memory tracking is now always available when enabled\n\n");
        found_deprecated = true;
    #endif
    
    #ifdef CONFIG_LEGACY_QUEUE_API
        printf("DEPRECATED CONFIG: CONFIG_LEGACY_QUEUE_API\n");
        printf("  Standard queue API now includes all legacy functionality\n\n");
        found_deprecated = true;
    #endif
    
    #ifdef CONFIG_SIMPLE_SCHEDULER
        printf("DEPRECATED CONFIG: CONFIG_SIMPLE_SCHEDULER\n");
        printf("  Use CONFIG_ENABLE_MULTI_CORE=n for single-core operation\n\n");
        found_deprecated = true;
    #endif
    
    // Print detailed deprecation information for all deprecated features
    for (size_t i = 0; i < PICO_RTOS_DEPRECATION_COUNT; i++) {
        print_deprecation_warning(&pico_rtos_deprecation_schedule[i]);
        found_deprecated = true;
    }
    
    if (!found_deprecated) {
        printf("No deprecated features detected in current configuration.\n");
        printf("Your configuration is up-to-date with v0.3.1 standards.\n\n");
    } else {
        printf("Please update your code to use the recommended replacements.\n");
        printf("See the migration guide for detailed instructions: %s\n", PICO_RTOS_MIGRATION_GUIDE_URL);
    }
    
    printf("=== End Deprecation Warnings ===\n\n");
}

/**
 * @brief Runtime deprecation check function
 * This can be called by deprecated functions to log warnings
 */
void pico_rtos_log_deprecation_warning(const char *function_name, const char *replacement) {
    #ifdef PICO_RTOS_ENABLE_LOGGING
        char warning_msg[256];
        snprintf(warning_msg, sizeof(warning_msg),
                "DEPRECATED: %s is deprecated. Use %s instead. See migration guide: %s", 
                function_name, replacement, PICO_RTOS_MIGRATION_GUIDE_URL);
        pico_rtos_simple_log(warning_msg);
    #else
        printf("DEPRECATED: %s is deprecated. Use %s instead.\n", 
               function_name, replacement);
    #endif
}

/**
 * @brief Check for problematic configuration combinations
 */
void pico_rtos_check_configuration_warnings(void) {
    #ifdef PICO_RTOS_ENABLE_MIGRATION_WARNINGS
    
    // Check for high tick rates
    #if defined(PICO_RTOS_TICK_RATE_HZ) && (PICO_RTOS_TICK_RATE_HZ > 2000)
        printf("CONFIG WARNING: High tick rate (%d Hz) may impact performance.\n", PICO_RTOS_TICK_RATE_HZ);
        printf("  Consider using CONFIG_ENABLE_HIRES_TIMERS for precise timing.\n\n");
    #endif
    
    // Check for high task counts
    #if defined(PICO_RTOS_MAX_TASKS) && (PICO_RTOS_MAX_TASKS > 32)
        printf("CONFIG WARNING: High task count (%d) may impact performance.\n", PICO_RTOS_MAX_TASKS);
        printf("  Consider task consolidation or enabling CONFIG_ENABLE_MULTI_CORE.\n\n");
    #endif
    
    // Check for multi-core without recommended features
    #if defined(PICO_RTOS_ENABLE_MULTI_CORE) && !defined(PICO_RTOS_ENABLE_MEMORY_TRACKING)
        printf("CONFIG RECOMMENDATION: Multi-core support works best with memory tracking.\n");
        printf("  Consider enabling CONFIG_ENABLE_MEMORY_TRACKING.\n\n");
    #endif
    
    #if defined(PICO_RTOS_ENABLE_MULTI_CORE) && !defined(PICO_RTOS_ENABLE_RUNTIME_STATS)
        printf("CONFIG RECOMMENDATION: Multi-core support requires runtime statistics.\n");
        printf("  Consider enabling CONFIG_ENABLE_RUNTIME_STATS for load balancing.\n\n");
    #endif
    
    // Check for debug features in production
    #if defined(PICO_RTOS_ENABLE_EXECUTION_PROFILING) && defined(PICO_RTOS_ENABLE_SYSTEM_TRACING) && defined(PICO_RTOS_ENABLE_DEADLOCK_DETECTION)
        printf("PERFORMANCE WARNING: Multiple debug features enabled simultaneously.\n");
        printf("  This may impact real-time performance. Consider disabling unused features.\n\n");
    #endif
    
    // Check for missing new features that might be beneficial
    #ifndef PICO_RTOS_ENABLE_EVENT_GROUPS
        printf("FEATURE SUGGESTION: Event groups provide advanced synchronization.\n");
        printf("  Consider enabling CONFIG_ENABLE_EVENT_GROUPS for multi-event coordination.\n\n");
    #endif
    
    #ifndef PICO_RTOS_ENABLE_STREAM_BUFFERS
        printf("FEATURE SUGGESTION: Stream buffers provide efficient variable-length messaging.\n");
        printf("  Consider enabling CONFIG_ENABLE_STREAM_BUFFERS for data streaming.\n\n");
    #endif
    
    #ifndef PICO_RTOS_ENABLE_MEMORY_POOLS
        printf("FEATURE SUGGESTION: Memory pools provide deterministic allocation.\n");
        printf("  Consider enabling CONFIG_ENABLE_MEMORY_POOLS for O(1) fixed-size allocation.\n\n");
    #endif
    
    #ifndef PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING
        printf("FEATURE SUGGESTION: System health monitoring provides production diagnostics.\n");
        printf("  Consider enabling CONFIG_ENABLE_SYSTEM_HEALTH_MONITORING.\n\n");
    #endif
    
    #endif // PICO_RTOS_ENABLE_MIGRATION_WARNINGS
}

/**
 * @brief Initialize deprecation warning system
 * Call this during RTOS initialization to show deprecation warnings
 */
void pico_rtos_deprecation_init(void) {
    #ifdef PICO_RTOS_ENABLE_MIGRATION_WARNINGS
        // Print deprecation warnings if any deprecated features are used
        static bool warnings_shown = false;
        if (!warnings_shown) {
            pico_rtos_print_deprecation_warnings();
            pico_rtos_check_configuration_warnings();
            warnings_shown = true;
        }
    #endif
}