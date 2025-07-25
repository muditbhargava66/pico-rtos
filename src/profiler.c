#include "pico_rtos/profiler.h"
#include "pico_rtos/config.h"
#include "pico_rtos.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if PICO_RTOS_ENABLE_EXECUTION_PROFILING

// =============================================================================
// INTERNAL CONSTANTS AND MACROS
// =============================================================================

#define PROFILER_MAGIC_NUMBER 0x50524F46  ///< "PROF" in hex
#define INVALID_FUNCTION_ID 0xFFFFFFFF    ///< Invalid function ID marker

// =============================================================================
// INTERNAL VARIABLES
// =============================================================================

static pico_rtos_profiler_t profiler = {0};
static pico_rtos_profile_entry_t *profile_entries = NULL;
static bool profiler_initialized = false;

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current time in microseconds with high resolution
 */
static inline uint64_t get_time_us(void) {
    return time_us_64();
}

/**
 * @brief Find profiling entry by function ID
 */
static pico_rtos_profile_entry_t *find_entry_by_id(uint32_t function_id) {
    if (!profile_entries || function_id == INVALID_FUNCTION_ID) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < profiler.max_entries; i++) {
        if (profile_entries[i].active && profile_entries[i].function_id == function_id) {
            return &profile_entries[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Find or create profiling entry for function ID
 */
static pico_rtos_profile_entry_t *get_or_create_entry(uint32_t function_id, const char *function_name) {
    if (!profile_entries || function_id == INVALID_FUNCTION_ID) {
        return NULL;
    }
    
    // First try to find existing entry
    pico_rtos_profile_entry_t *entry = find_entry_by_id(function_id);
    if (entry) {
        return entry;
    }
    
    // Find free slot
    for (uint32_t i = 0; i < profiler.max_entries; i++) {
        if (!profile_entries[i].active) {
            entry = &profile_entries[i];
            
            // Initialize new entry
            memset(entry, 0, sizeof(pico_rtos_profile_entry_t));
            entry->function_id = function_id;
            entry->function_name = function_name;
            entry->min_time_us = UINT32_MAX;
            entry->max_time_us = 0;
            entry->active = true;
            
            profiler.active_entries++;
            return entry;
        }
    }
    
    // No free slots available
    profiler.overflow_count++;
    return NULL;
}

/**
 * @brief Update profiling statistics for an entry
 */
static void update_entry_stats(pico_rtos_profile_entry_t *entry, uint32_t execution_time_us) {
    if (!entry) {
        return;
    }
    
    entry->call_count++;
    entry->total_time_us += execution_time_us;
    entry->last_call_time = get_time_us();
    
    if (execution_time_us < entry->min_time_us) {
        entry->min_time_us = execution_time_us;
    }
    
    if (execution_time_us > entry->max_time_us) {
        entry->max_time_us = execution_time_us;
    }
    
    // Calculate average
    entry->avg_time_us = (uint32_t)(entry->total_time_us / entry->call_count);
}

/**
 * @brief Compare function for sorting by average execution time (descending)
 */
static int compare_by_avg_time_desc(const void *a, const void *b) {
    const pico_rtos_profile_entry_t *entry_a = (const pico_rtos_profile_entry_t *)a;
    const pico_rtos_profile_entry_t *entry_b = (const pico_rtos_profile_entry_t *)b;
    
    if (entry_a->avg_time_us > entry_b->avg_time_us) return -1;
    if (entry_a->avg_time_us < entry_b->avg_time_us) return 1;
    return 0;
}

/**
 * @brief Compare function for sorting by call count (descending)
 */
static int compare_by_call_count_desc(const void *a, const void *b) {
    const pico_rtos_profile_entry_t *entry_a = (const pico_rtos_profile_entry_t *)a;
    const pico_rtos_profile_entry_t *entry_b = (const pico_rtos_profile_entry_t *)b;
    
    if (entry_a->call_count > entry_b->call_count) return -1;
    if (entry_a->call_count < entry_b->call_count) return 1;
    return 0;
}

// =============================================================================
// PROFILER CONTROL IMPLEMENTATION
// =============================================================================

bool pico_rtos_profiler_init(uint32_t max_entries) {
    if (profiler_initialized) {
        return false; // Already initialized
    }
    
    if (max_entries == 0 || max_entries > PICO_RTOS_PROFILING_MAX_ENTRIES) {
        max_entries = PICO_RTOS_PROFILING_MAX_ENTRIES;
    }
    
    // Allocate memory for profiling entries
    profile_entries = (pico_rtos_profile_entry_t *)pico_rtos_malloc(
        max_entries * sizeof(pico_rtos_profile_entry_t));
    
    if (!profile_entries) {
        return false;
    }
    
    // Initialize profiler structure
    memset(&profiler, 0, sizeof(pico_rtos_profiler_t));
    profiler.entries = profile_entries;
    profiler.max_entries = max_entries;
    profiler.profiling_enabled = true;
    profiler.start_time = get_time_us();
    
    // Clear all entries
    memset(profile_entries, 0, max_entries * sizeof(pico_rtos_profile_entry_t));
    
    profiler_initialized = true;
    return true;
}

void pico_rtos_profiler_deinit(void) {
    if (!profiler_initialized) {
        return;
    }
    
    profiler.profiling_enabled = false;
    
    if (profile_entries) {
        pico_rtos_free(profile_entries, profiler.max_entries * sizeof(pico_rtos_profile_entry_t));
        profile_entries = NULL;
    }
    
    memset(&profiler, 0, sizeof(pico_rtos_profiler_t));
    profiler_initialized = false;
}

void pico_rtos_profiler_enable(bool enable) {
    if (profiler_initialized) {
        profiler.profiling_enabled = enable;
    }
}

bool pico_rtos_profiler_is_enabled(void) {
    return profiler_initialized && profiler.profiling_enabled;
}

void pico_rtos_profiler_reset(void) {
    if (!profiler_initialized || !profile_entries) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    // Clear all entries
    memset(profile_entries, 0, profiler.max_entries * sizeof(pico_rtos_profile_entry_t));
    
    // Reset profiler statistics
    profiler.active_entries = 0;
    profiler.start_time = get_time_us();
    profiler.total_overhead_us = 0;
    profiler.overflow_count = 0;
    
    pico_rtos_exit_critical();
}

bool pico_rtos_profiler_reset_function(uint32_t function_id) {
    if (!profiler_initialized || !profiler.profiling_enabled) {
        return false;
    }
    
    pico_rtos_enter_critical();
    
    pico_rtos_profile_entry_t *entry = find_entry_by_id(function_id);
    if (entry) {
        // Keep function_id and name, reset statistics
        uint32_t id = entry->function_id;
        const char *name = entry->function_name;
        
        memset(entry, 0, sizeof(pico_rtos_profile_entry_t));
        entry->function_id = id;
        entry->function_name = name;
        entry->min_time_us = UINT32_MAX;
        entry->active = true;
        
        pico_rtos_exit_critical();
        return true;
    }
    
    pico_rtos_exit_critical();
    return false;
}

// =============================================================================
// PROFILING MEASUREMENT IMPLEMENTATION
// =============================================================================

bool pico_rtos_profiler_function_enter(uint32_t function_id, const char *function_name,
                                      pico_rtos_profile_context_t *context) {
    if (!profiler_initialized || !profiler.profiling_enabled || !context) {
        if (context) {
            context->valid = false;
        }
        return false;
    }
    
    uint64_t start_time = get_time_us();
    
    // Initialize context
    context->function_id = function_id;
    context->start_time = start_time;
    context->valid = true;
    
    // Ensure entry exists (this may allocate a new entry)
    pico_rtos_profile_entry_t *entry = get_or_create_entry(function_id, function_name);
    if (!entry) {
        context->valid = false;
        return false;
    }
    
    // Account for profiling overhead
    uint64_t end_time = get_time_us();
    profiler.total_overhead_us += (end_time - start_time);
    
    return true;
}

bool pico_rtos_profiler_function_exit(pico_rtos_profile_context_t *context) {
    if (!profiler_initialized || !profiler.profiling_enabled || !context || !context->valid) {
        return false;
    }
    
    uint64_t end_time = get_time_us();
    uint64_t overhead_start = end_time;
    
    // Calculate execution time
    uint64_t execution_time = end_time - context->start_time;
    
    // Subtract profiling overhead
    if (execution_time > PICO_RTOS_PROFILING_OVERHEAD_US) {
        execution_time -= PICO_RTOS_PROFILING_OVERHEAD_US;
    }
    
    // Find the entry and update statistics
    pico_rtos_profile_entry_t *entry = find_entry_by_id(context->function_id);
    if (entry) {
        update_entry_stats(entry, (uint32_t)execution_time);
    }
    
    // Mark context as invalid
    context->valid = false;
    
    // Account for profiling overhead
    uint64_t overhead_end = get_time_us();
    profiler.total_overhead_us += (overhead_end - overhead_start);
    
    return true;
}

bool pico_rtos_profiler_record_time(uint32_t function_id, const char *function_name,
                                   uint32_t execution_time_us) {
    if (!profiler_initialized || !profiler.profiling_enabled) {
        return false;
    }
    
    uint64_t overhead_start = get_time_us();
    
    pico_rtos_profile_entry_t *entry = get_or_create_entry(function_id, function_name);
    if (entry) {
        update_entry_stats(entry, execution_time_us);
    }
    
    // Account for profiling overhead
    uint64_t overhead_end = get_time_us();
    profiler.total_overhead_us += (overhead_end - overhead_start);
    
    return entry != NULL;
}

// =============================================================================
// PROFILING DATA RETRIEVAL IMPLEMENTATION
// =============================================================================

bool pico_rtos_profiler_get_entry(uint32_t function_id, pico_rtos_profile_entry_t *entry) {
    if (!profiler_initialized || !entry) {
        return false;
    }
    
    pico_rtos_enter_critical();
    
    pico_rtos_profile_entry_t *found_entry = find_entry_by_id(function_id);
    if (found_entry) {
        *entry = *found_entry;
        pico_rtos_exit_critical();
        return true;
    }
    
    pico_rtos_exit_critical();
    return false;
}

uint32_t pico_rtos_profiler_get_all_entries(pico_rtos_profile_entry_t *entries, uint32_t max_entries) {
    if (!profiler_initialized || !entries || max_entries == 0) {
        return 0;
    }
    
    uint32_t count = 0;
    
    pico_rtos_enter_critical();
    
    for (uint32_t i = 0; i < profiler.max_entries && count < max_entries; i++) {
        if (profile_entries[i].active) {
            entries[count] = profile_entries[i];
            count++;
        }
    }
    
    pico_rtos_exit_critical();
    
    return count;
}

bool pico_rtos_profiler_get_stats(pico_rtos_profiling_stats_t *stats) {
    if (!profiler_initialized || !stats) {
        return false;
    }
    
    memset(stats, 0, sizeof(pico_rtos_profiling_stats_t));
    
    pico_rtos_enter_critical();
    
    uint32_t slowest_avg = 0;
    uint32_t fastest_avg = UINT32_MAX;
    uint32_t most_calls = 0;
    
    for (uint32_t i = 0; i < profiler.max_entries; i++) {
        if (profile_entries[i].active) {
            stats->total_functions++;
            stats->total_calls += profile_entries[i].call_count;
            stats->total_execution_time_us += profile_entries[i].total_time_us;
            
            if (profile_entries[i].avg_time_us > slowest_avg) {
                slowest_avg = profile_entries[i].avg_time_us;
                stats->slowest_function_id = profile_entries[i].function_id;
            }
            
            if (profile_entries[i].avg_time_us < fastest_avg) {
                fastest_avg = profile_entries[i].avg_time_us;
                stats->fastest_function_id = profile_entries[i].function_id;
            }
            
            if (profile_entries[i].call_count > most_calls) {
                most_calls = profile_entries[i].call_count;
                stats->most_called_function_id = profile_entries[i].function_id;
            }
        }
    }
    
    stats->total_profiling_time_us = get_time_us() - profiler.start_time;
    stats->overflow_count = profiler.overflow_count;
    
    // Calculate profiling overhead percentage
    if (stats->total_profiling_time_us > 0) {
        stats->profiling_overhead_percent = 
            (float)profiler.total_overhead_us / stats->total_profiling_time_us * 100.0f;
    }
    
    pico_rtos_exit_critical();
    
    return true;
}

uint32_t pico_rtos_profiler_get_slowest_functions(pico_rtos_profile_entry_t *entries, uint32_t max_entries) {
    if (!profiler_initialized || !entries || max_entries == 0) {
        return 0;
    }
    
    // Get all entries first
    uint32_t total_entries = pico_rtos_profiler_get_all_entries(entries, max_entries);
    
    if (total_entries == 0) {
        return 0;
    }
    
    // Sort by average execution time (descending)
    qsort(entries, total_entries, sizeof(pico_rtos_profile_entry_t), compare_by_avg_time_desc);
    
    return total_entries;
}

uint32_t pico_rtos_profiler_get_most_called_functions(pico_rtos_profile_entry_t *entries, uint32_t max_entries) {
    if (!profiler_initialized || !entries || max_entries == 0) {
        return 0;
    }
    
    // Get all entries first
    uint32_t total_entries = pico_rtos_profiler_get_all_entries(entries, max_entries);
    
    if (total_entries == 0) {
        return 0;
    }
    
    // Sort by call count (descending)
    qsort(entries, total_entries, sizeof(pico_rtos_profile_entry_t), compare_by_call_count_desc);
    
    return total_entries;
}

// =============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// =============================================================================

uint32_t pico_rtos_profiler_format_entry(const pico_rtos_profile_entry_t *entry, 
                                         char *buffer, uint32_t buffer_size) {
    if (!entry || !buffer || buffer_size == 0) {
        return 0;
    }
    
    const char *name = entry->function_name ? entry->function_name : "Unknown";
    
    return snprintf(buffer, buffer_size,
        "Function: %s (ID: 0x%08X)\n"
        "  Calls: %u\n"
        "  Total Time: %llu us\n"
        "  Avg Time: %u us\n"
        "  Min Time: %u us\n"
        "  Max Time: %u us\n"
        "  Last Call: %llu us ago\n",
        name, entry->function_id,
        entry->call_count,
        entry->total_time_us,
        entry->avg_time_us,
        entry->min_time_us,
        entry->max_time_us,
        get_time_us() - entry->last_call_time
    );
}

void pico_rtos_profiler_print_entry(const pico_rtos_profile_entry_t *entry) {
    if (!entry) {
        return;
    }
    
    char buffer[512];
    pico_rtos_profiler_format_entry(entry, buffer, sizeof(buffer));
    printf("%s", buffer);
}

void pico_rtos_profiler_print_stats(const pico_rtos_profiling_stats_t *stats) {
    if (!stats) {
        return;
    }
    
    printf("Profiling Statistics Summary:\n");
    printf("  Total Functions: %u\n", stats->total_functions);
    printf("  Total Calls: %u\n", stats->total_calls);
    printf("  Total Execution Time: %llu us\n", stats->total_execution_time_us);
    printf("  Total Profiling Time: %llu us\n", stats->total_profiling_time_us);
    printf("  Profiling Overhead: %.2f%%\n", stats->profiling_overhead_percent);
    printf("  Slowest Function ID: 0x%08X\n", stats->slowest_function_id);
    printf("  Fastest Function ID: 0x%08X\n", stats->fastest_function_id);
    printf("  Most Called Function ID: 0x%08X\n", stats->most_called_function_id);
    printf("  Entry Overflows: %u\n", stats->overflow_count);
}

void pico_rtos_profiler_print_all_entries(void) {
    if (!profiler_initialized) {
        printf("Profiler not initialized\n");
        return;
    }
    
    printf("Profiling Entries:\n");
    printf("==================\n");
    
    pico_rtos_profile_entry_t entries[PICO_RTOS_PROFILING_MAX_ENTRIES];
    uint32_t count = pico_rtos_profiler_get_all_entries(entries, PICO_RTOS_PROFILING_MAX_ENTRIES);
    
    if (count == 0) {
        printf("No profiling data available\n");
        return;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        printf("Entry %u:\n", i + 1);
        pico_rtos_profiler_print_entry(&entries[i]);
        printf("\n");
    }
}

void pico_rtos_profiler_cleanup_context(pico_rtos_profile_context_t **context) {
    if (context && *context && (*context)->valid) {
        pico_rtos_profiler_function_exit(*context);
    }
}

#endif // PICO_RTOS_ENABLE_EXECUTION_PROFILING