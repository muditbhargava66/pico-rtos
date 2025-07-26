#ifndef PICO_RTOS_PROFILER_H
#define PICO_RTOS_PROFILER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file profiler.h
 * @brief Pico-RTOS Execution Time Profiling System
 * 
 * This module provides high-resolution execution time profiling using
 * RP2040 hardware timers with minimal overhead for performance analysis.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

/**
 * @brief Enable execution profiling
 * 
 * When disabled, all profiling functions compile to empty stubs with zero overhead.
 */
#ifndef PICO_RTOS_ENABLE_EXECUTION_PROFILING
#define PICO_RTOS_ENABLE_EXECUTION_PROFILING 0
#endif

/**
 * @brief Maximum number of profiling entries
 */
#ifndef PICO_RTOS_PROFILING_MAX_ENTRIES
#define PICO_RTOS_PROFILING_MAX_ENTRIES 64
#endif

/**
 * @brief Profiling overhead compensation in microseconds
 * 
 * This value is subtracted from measurements to account for profiling overhead.
 */
#ifndef PICO_RTOS_PROFILING_OVERHEAD_US
#define PICO_RTOS_PROFILING_OVERHEAD_US 2
#endif

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Profiling entry for a function or code section
 */
typedef struct {
    uint32_t function_id;                  ///< Unique function identifier
    const char *function_name;             ///< Function name (optional)
    uint32_t call_count;                   ///< Number of calls
    uint64_t total_time_us;                ///< Total execution time in microseconds
    uint32_t min_time_us;                  ///< Minimum execution time
    uint32_t max_time_us;                  ///< Maximum execution time
    uint32_t avg_time_us;                  ///< Average execution time
    uint64_t last_call_time;               ///< Timestamp of last call
    bool active;                           ///< Entry is active/in use
} pico_rtos_profile_entry_t;

/**
 * @brief Profiler system state
 */
typedef struct {
    pico_rtos_profile_entry_t *entries;    ///< Profile entries array
    uint32_t max_entries;                  ///< Maximum number of entries
    uint32_t active_entries;               ///< Currently active entries
    bool profiling_enabled;                ///< Global profiling enable
    uint64_t start_time;                   ///< Profiling start timestamp
    uint64_t total_overhead_us;            ///< Total profiling overhead
    uint32_t overflow_count;               ///< Number of entry overflows
} pico_rtos_profiler_t;

/**
 * @brief Profiling session context for function entry/exit
 */
typedef struct {
    uint32_t function_id;                  ///< Function being profiled
    uint64_t start_time;                   ///< Function start time
    bool valid;                            ///< Context is valid
} pico_rtos_profile_context_t;

/**
 * @brief Profiling statistics summary
 */
typedef struct {
    uint32_t total_functions;              ///< Total functions profiled
    uint32_t total_calls;                  ///< Total function calls
    uint64_t total_execution_time_us;      ///< Total execution time
    uint64_t total_profiling_time_us;      ///< Total time spent profiling
    uint32_t slowest_function_id;          ///< ID of slowest function
    uint32_t fastest_function_id;          ///< ID of fastest function
    uint32_t most_called_function_id;      ///< ID of most called function
    float profiling_overhead_percent;      ///< Profiling overhead percentage
    uint32_t overflow_count;               ///< Number of entry overflows
} pico_rtos_profiling_stats_t;

// =============================================================================
// PROFILER CONTROL API
// =============================================================================

#if PICO_RTOS_ENABLE_EXECUTION_PROFILING

/**
 * @brief Initialize the profiling system
 * 
 * Sets up the profiling system with the specified maximum number of entries.
 * Must be called before any other profiling functions.
 * 
 * @param max_entries Maximum number of profiling entries
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_profiler_init(uint32_t max_entries);

/**
 * @brief Deinitialize the profiling system
 * 
 * Cleans up profiling resources and disables profiling.
 */
void pico_rtos_profiler_deinit(void);

/**
 * @brief Enable profiling
 * 
 * Enables the profiling system. When disabled, profiling calls have minimal overhead.
 * 
 * @param enable true to enable profiling, false to disable
 */
void pico_rtos_profiler_enable(bool enable);

/**
 * @brief Check if profiling is enabled
 * 
 * @return true if profiling is enabled, false otherwise
 */
bool pico_rtos_profiler_is_enabled(void);

/**
 * @brief Reset all profiling data
 * 
 * Clears all profiling entries and resets statistics.
 */
void pico_rtos_profiler_reset(void);

/**
 * @brief Reset profiling data for a specific function
 * 
 * Clears profiling data for the specified function ID.
 * 
 * @param function_id Function ID to reset
 * @return true if function was found and reset, false otherwise
 */
bool pico_rtos_profiler_reset_function(uint32_t function_id);

// =============================================================================
// PROFILING MEASUREMENT API
// =============================================================================

/**
 * @brief Start profiling a function
 * 
 * Records the start time for a function. Should be called at function entry.
 * 
 * @param function_id Unique function identifier
 * @param function_name Function name (optional, can be NULL)
 * @param context Pointer to context structure to store session data
 * @return true if profiling was started successfully, false otherwise
 */
bool pico_rtos_profiler_function_enter(uint32_t function_id, const char *function_name,
                                      pico_rtos_profile_context_t *context);

/**
 * @brief End profiling a function
 * 
 * Records the end time and updates profiling statistics for a function.
 * Should be called at function exit.
 * 
 * @param context Pointer to context structure from function_enter
 * @return true if profiling was ended successfully, false otherwise
 */
bool pico_rtos_profiler_function_exit(pico_rtos_profile_context_t *context);

/**
 * @brief Profile a code section with manual timing
 * 
 * Manually records execution time for a code section.
 * 
 * @param function_id Unique identifier for the code section
 * @param function_name Name of the code section (optional)
 * @param execution_time_us Execution time in microseconds
 * @return true if profiling was recorded successfully, false otherwise
 */
bool pico_rtos_profiler_record_time(uint32_t function_id, const char *function_name,
                                   uint32_t execution_time_us);

// =============================================================================
// PROFILING DATA RETRIEVAL API
// =============================================================================

/**
 * @brief Get profiling entry for a specific function
 * 
 * Retrieves profiling data for the specified function ID.
 * 
 * @param function_id Function ID to retrieve
 * @param entry Pointer to structure to fill with profiling data
 * @return true if function was found, false otherwise
 */
bool pico_rtos_profiler_get_entry(uint32_t function_id, pico_rtos_profile_entry_t *entry);

/**
 * @brief Get all profiling entries
 * 
 * Retrieves profiling data for all active functions.
 * 
 * @param entries Array to fill with profiling entries
 * @param max_entries Maximum number of entries to retrieve
 * @return Number of entries actually retrieved
 */
uint32_t pico_rtos_profiler_get_all_entries(pico_rtos_profile_entry_t *entries, uint32_t max_entries);

/**
 * @brief Get profiling statistics summary
 * 
 * Provides a summary of profiling statistics across all functions.
 * 
 * @param stats Pointer to structure to fill with statistics
 * @return true if statistics were retrieved successfully, false otherwise
 */
bool pico_rtos_profiler_get_stats(pico_rtos_profiling_stats_t *stats);

/**
 * @brief Get the top N slowest functions
 * 
 * Returns the functions with the highest average execution times.
 * 
 * @param entries Array to fill with top functions
 * @param max_entries Maximum number of entries to return
 * @return Number of entries actually returned
 */
uint32_t pico_rtos_profiler_get_slowest_functions(pico_rtos_profile_entry_t *entries, uint32_t max_entries);

/**
 * @brief Get the top N most called functions
 * 
 * Returns the functions with the highest call counts.
 * 
 * @param entries Array to fill with top functions
 * @param max_entries Maximum number of entries to return
 * @return Number of entries actually returned
 */
uint32_t pico_rtos_profiler_get_most_called_functions(pico_rtos_profile_entry_t *entries, uint32_t max_entries);

// =============================================================================
// PROFILING MACROS FOR EASY INSTRUMENTATION
// =============================================================================

/**
 * @brief Macro to generate unique function IDs
 * 
 * Uses file name and line number to create a unique ID.
 */
#define PICO_RTOS_PROFILE_ID() ((uint32_t)(__LINE__ ^ (((uint32_t)__FILE__[0]) << 8) ^ (((uint32_t)__FILE__[1]) << 16)))

/**
 * @brief Macro to profile a function automatically
 * 
 * Place this at the beginning of a function to automatically profile it.
 * The profiling context will be automatically cleaned up when the function exits.
 */
#define PICO_RTOS_PROFILE_FUNCTION() \
    pico_rtos_profile_context_t __profile_ctx; \
    pico_rtos_profiler_function_enter(PICO_RTOS_PROFILE_ID(), __func__, &__profile_ctx); \
    __attribute__((cleanup(pico_rtos_profiler_cleanup_context))) \
    pico_rtos_profile_context_t *__profile_cleanup = &__profile_ctx

/**
 * @brief Macro to profile a code block
 * 
 * Use this to profile a specific code block within a function.
 */
#define PICO_RTOS_PROFILE_BLOCK_START(name) \
    do { \
        pico_rtos_profile_context_t __block_ctx; \
        pico_rtos_profiler_function_enter(PICO_RTOS_PROFILE_ID(), name, &__block_ctx)

#define PICO_RTOS_PROFILE_BLOCK_END() \
        pico_rtos_profiler_function_exit(&__block_ctx); \
    } while(0)

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Format profiling entry as string
 * 
 * Formats profiling entry information into a human-readable string.
 * 
 * @param entry Profiling entry to format
 * @param buffer Buffer to store formatted string
 * @param buffer_size Size of the buffer
 * @return Number of characters written (excluding null terminator)
 */
uint32_t pico_rtos_profiler_format_entry(const pico_rtos_profile_entry_t *entry, 
                                         char *buffer, uint32_t buffer_size);

/**
 * @brief Print profiling entry to console
 * 
 * Prints formatted profiling entry information to the console.
 * 
 * @param entry Profiling entry to print
 */
void pico_rtos_profiler_print_entry(const pico_rtos_profile_entry_t *entry);

/**
 * @brief Print profiling statistics to console
 * 
 * Prints formatted profiling statistics summary to the console.
 * 
 * @param stats Profiling statistics to print
 */
void pico_rtos_profiler_print_stats(const pico_rtos_profiling_stats_t *stats);

/**
 * @brief Print all profiling entries to console
 * 
 * Prints all active profiling entries in a formatted table.
 */
void pico_rtos_profiler_print_all_entries(void);

/**
 * @brief Cleanup function for automatic profiling context
 * 
 * Internal function used by profiling macros for automatic cleanup.
 * 
 * @param context Pointer to profiling context to clean up
 */
void pico_rtos_profiler_cleanup_context(pico_rtos_profile_context_t **context);

#else // PICO_RTOS_ENABLE_EXECUTION_PROFILING

// When profiling is disabled, all functions become empty stubs
#define pico_rtos_profiler_init(max_entries) (true)
#define pico_rtos_profiler_deinit() ((void)0)
#define pico_rtos_profiler_enable(enable) ((void)0)
#define pico_rtos_profiler_is_enabled() (false)
#define pico_rtos_profiler_reset() ((void)0)
#define pico_rtos_profiler_reset_function(function_id) (false)
#define pico_rtos_profiler_function_enter(id, name, ctx) (false)
#define pico_rtos_profiler_function_exit(ctx) (false)
#define pico_rtos_profiler_record_time(id, name, time) (false)
#define pico_rtos_profiler_get_entry(id, entry) (false)
#define pico_rtos_profiler_get_all_entries(entries, max) (0)
#define pico_rtos_profiler_get_stats(stats) (false)
#define pico_rtos_profiler_get_slowest_functions(entries, max) (0)
#define pico_rtos_profiler_get_most_called_functions(entries, max) (0)
#define pico_rtos_profiler_format_entry(entry, buffer, size) (0)
#define pico_rtos_profiler_print_entry(entry) ((void)0)
#define pico_rtos_profiler_print_stats(stats) ((void)0)
#define pico_rtos_profiler_print_all_entries() ((void)0)
#define pico_rtos_profiler_cleanup_context(ctx) ((void)0)

// Profiling macros become no-ops
#define PICO_RTOS_PROFILE_ID() (0)
#define PICO_RTOS_PROFILE_FUNCTION() ((void)0)
#define PICO_RTOS_PROFILE_BLOCK_START(name) do {
#define PICO_RTOS_PROFILE_BLOCK_END() } while(0)

#endif // PICO_RTOS_ENABLE_EXECUTION_PROFILING

#endif // PICO_RTOS_PROFILER_H