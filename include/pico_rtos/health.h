#ifndef PICO_RTOS_HEALTH_H
#define PICO_RTOS_HEALTH_H

#include <stdint.h>
#include <stdbool.h>
#include "pico_rtos/config.h"
#include "pico_rtos/types.h"
#include "pico/critical_section.h"

/**
 * @file health.h
 * @brief Comprehensive System Health Monitoring for Pico-RTOS
 * 
 * This module provides comprehensive system health monitoring capabilities,
 * tracking CPU usage, memory usage, task performance, and system metrics
 * with configurable thresholds and alerting.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef PICO_RTOS_HEALTH_SAMPLE_INTERVAL_MS
#define PICO_RTOS_HEALTH_SAMPLE_INTERVAL_MS 100
#endif

#ifndef PICO_RTOS_HEALTH_HISTORY_SIZE
#define PICO_RTOS_HEALTH_HISTORY_SIZE 60
#endif

#ifndef PICO_RTOS_HEALTH_MAX_ALERTS
#define PICO_RTOS_HEALTH_MAX_ALERTS 16
#endif

#ifndef PICO_RTOS_HEALTH_ENABLE_LEAK_DETECTION
#define PICO_RTOS_HEALTH_ENABLE_LEAK_DETECTION 1
#endif

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

/**
 * @brief Health monitoring states
 */
typedef enum {
    PICO_RTOS_HEALTH_STATE_HEALTHY = 0,
    PICO_RTOS_HEALTH_STATE_WARNING,
    PICO_RTOS_HEALTH_STATE_CRITICAL,
    PICO_RTOS_HEALTH_STATE_FAILURE
} pico_rtos_health_state_t;

/**
 * @brief Health metric types
 */
typedef enum {
    PICO_RTOS_HEALTH_METRIC_CPU_USAGE = 0,
    PICO_RTOS_HEALTH_METRIC_MEMORY_USAGE,
    PICO_RTOS_HEALTH_METRIC_STACK_USAGE,
    PICO_RTOS_HEALTH_METRIC_TASK_RUNTIME,
    PICO_RTOS_HEALTH_METRIC_INTERRUPT_LATENCY,
    PICO_RTOS_HEALTH_METRIC_CONTEXT_SWITCHES,
    PICO_RTOS_HEALTH_METRIC_QUEUE_USAGE,
    PICO_RTOS_HEALTH_METRIC_MUTEX_CONTENTION,
    PICO_RTOS_HEALTH_METRIC_CUSTOM
} pico_rtos_health_metric_type_t;

/**
 * @brief Alert severity levels
 */
typedef enum {
    PICO_RTOS_HEALTH_ALERT_INFO = 0,
    PICO_RTOS_HEALTH_ALERT_WARNING,
    PICO_RTOS_HEALTH_ALERT_ERROR,
    PICO_RTOS_HEALTH_ALERT_CRITICAL
} pico_rtos_health_alert_level_t;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct pico_rtos_task pico_rtos_task_t;
typedef struct pico_rtos_health_monitor pico_rtos_health_monitor_t;
typedef struct pico_rtos_health_metric pico_rtos_health_metric_t;
typedef struct pico_rtos_health_alert pico_rtos_health_alert_t;

// =============================================================================
// CALLBACK TYPES
// =============================================================================

/**
 * @brief Health alert callback function type
 * 
 * Called when a health alert is triggered.
 * 
 * @param alert Pointer to alert information
 * @param user_data User-defined data
 */
typedef void (*pico_rtos_health_alert_callback_t)(const pico_rtos_health_alert_t *alert, 
                                                 void *user_data);

/**
 * @brief Custom metric collection callback
 * 
 * Called to collect custom metric values.
 * 
 * @param metric Pointer to metric structure
 * @param user_data User-defined data
 * @return Current metric value
 */
typedef uint32_t (*pico_rtos_health_metric_callback_t)(pico_rtos_health_metric_t *metric,
                                                      void *user_data);

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief CPU usage statistics per core
 */
typedef struct {
    uint32_t core_id;                           ///< Core identifier
    uint32_t usage_percent;                     ///< Current CPU usage percentage
    uint32_t idle_time_us;                      ///< Time spent idle (microseconds)
    uint32_t active_time_us;                    ///< Time spent active (microseconds)
    uint32_t interrupt_time_us;                 ///< Time spent in interrupts (microseconds)
    uint32_t context_switches;                  ///< Number of context switches
    uint32_t interrupts_handled;                ///< Number of interrupts handled
    uint64_t total_runtime_us;                  ///< Total runtime since boot
} pico_rtos_cpu_stats_t;

/**
 * @brief Memory usage statistics
 */
typedef struct {
    uint32_t total_heap_size;                   ///< Total heap size
    uint32_t used_heap_size;                    ///< Currently used heap
    uint32_t free_heap_size;                    ///< Currently free heap
    uint32_t peak_heap_usage;                   ///< Peak heap usage
    uint32_t heap_fragmentation_percent;        ///< Heap fragmentation percentage
    uint32_t total_allocations;                 ///< Total allocations made
    uint32_t total_deallocations;               ///< Total deallocations made
    uint32_t current_allocations;               ///< Current active allocations
    uint32_t allocation_failures;               ///< Number of failed allocations
    uint32_t largest_free_block;                ///< Largest contiguous free block
} pico_rtos_memory_stats_t;

/**
 * @brief Task performance statistics
 */
typedef struct {
    pico_rtos_task_t *task;                     ///< Task pointer
    const char *name;                           ///< Task name
    uint32_t priority;                          ///< Task priority
    pico_rtos_task_state_t state;               ///< Current task state
    uint32_t stack_size;                        ///< Total stack size
    uint32_t stack_used;                        ///< Stack space used
    uint32_t stack_high_water;                  ///< Stack high water mark
    uint32_t stack_usage_percent;               ///< Stack usage percentage
    uint64_t total_runtime_us;                  ///< Total runtime
    uint32_t cpu_usage_percent;                 ///< CPU usage percentage
    uint32_t context_switches;                  ///< Number of context switches
    uint32_t preemptions;                       ///< Number of preemptions
    uint32_t voluntary_yields;                  ///< Number of voluntary yields
    uint32_t blocked_time_us;                   ///< Time spent blocked
    uint32_t ready_time_us;                     ///< Time spent ready but not running
} pico_rtos_task_stats_t;

/**
 * @brief System-wide health metrics
 */
typedef struct {
    uint32_t uptime_ms;                         ///< System uptime in milliseconds
    uint32_t tick_count;                        ///< System tick count
    uint32_t tick_rate_hz;                      ///< System tick rate
    uint32_t total_tasks;                       ///< Total number of tasks
    uint32_t ready_tasks;                       ///< Number of ready tasks
    uint32_t blocked_tasks;                     ///< Number of blocked tasks
    uint32_t suspended_tasks;                   ///< Number of suspended tasks
    uint32_t terminated_tasks;                  ///< Number of terminated tasks
    uint32_t total_interrupts;                  ///< Total interrupts handled
    uint32_t missed_deadlines;                  ///< Number of missed deadlines
    uint32_t stack_overflows;                   ///< Number of stack overflows detected
    uint32_t assertion_failures;                ///< Number of assertion failures
    uint32_t error_count;                       ///< Total error count
    pico_rtos_health_state_t overall_health;    ///< Overall system health state
} pico_rtos_system_health_t;

/**
 * @brief Health metric structure
 */
struct pico_rtos_health_metric {
    uint32_t metric_id;                         ///< Unique metric identifier
    pico_rtos_health_metric_type_t type;        ///< Metric type
    const char *name;                           ///< Metric name
    const char *description;                    ///< Metric description
    const char *units;                          ///< Metric units
    
    uint32_t current_value;                     ///< Current metric value
    uint32_t min_value;                         ///< Minimum observed value
    uint32_t max_value;                         ///< Maximum observed value
    uint32_t average_value;                     ///< Average value
    uint32_t sample_count;                      ///< Number of samples collected
    
    // Thresholds
    uint32_t warning_threshold;                 ///< Warning threshold
    uint32_t critical_threshold;                ///< Critical threshold
    bool threshold_enabled;                     ///< Threshold checking enabled
    
    // History
    uint32_t history[PICO_RTOS_HEALTH_HISTORY_SIZE]; ///< Historical values
    uint32_t history_index;                     ///< Current history index
    bool history_full;                          ///< History buffer is full
    
    // Custom metric support
    pico_rtos_health_metric_callback_t callback; ///< Custom collection callback
    void *user_data;                            ///< User data for callback
    
    bool active;                                ///< Metric is active
    uint32_t last_update_time;                  ///< Last update timestamp
    critical_section_t cs;                      ///< Critical section for thread safety
};

/**
 * @brief Health alert structure
 */
struct pico_rtos_health_alert {
    uint32_t alert_id;                          ///< Unique alert identifier
    pico_rtos_health_alert_level_t level;       ///< Alert severity level
    pico_rtos_health_metric_type_t metric_type; ///< Related metric type
    uint32_t metric_id;                         ///< Related metric ID
    const char *message;                        ///< Alert message
    const char *description;                    ///< Detailed description
    uint32_t value;                             ///< Value that triggered alert
    uint32_t threshold;                         ///< Threshold that was exceeded
    uint32_t timestamp;                         ///< Alert timestamp
    bool acknowledged;                          ///< Alert has been acknowledged
    bool active;                                ///< Alert is currently active
};

/**
 * @brief Memory leak detection entry
 */
typedef struct {
    void *address;                              ///< Allocated memory address
    size_t size;                                ///< Allocation size
    uint32_t timestamp;                         ///< Allocation timestamp
    const char *file;                           ///< Source file
    int line;                                   ///< Source line
    const char *function;                       ///< Source function
    bool active;                                ///< Allocation is active
} pico_rtos_memory_leak_entry_t;

/**
 * @brief Health monitor main structure
 */
struct pico_rtos_health_monitor {
    bool initialized;                           ///< Monitor is initialized
    bool enabled;                               ///< Monitoring is enabled
    uint32_t sample_interval_ms;                ///< Sampling interval
    uint32_t last_sample_time;                  ///< Last sampling time
    
    // Metrics
    pico_rtos_health_metric_t metrics[32];      ///< Health metrics array
    uint32_t metric_count;                      ///< Number of active metrics
    uint32_t next_metric_id;                    ///< Next metric ID to assign
    
    // Alerts
    pico_rtos_health_alert_t alerts[PICO_RTOS_HEALTH_MAX_ALERTS]; ///< Active alerts
    uint32_t alert_count;                       ///< Number of active alerts
    uint32_t next_alert_id;                     ///< Next alert ID to assign
    pico_rtos_health_alert_callback_t alert_callback; ///< Alert callback
    void *alert_callback_data;                  ///< Alert callback user data
    
    // System statistics
    pico_rtos_cpu_stats_t cpu_stats[2];         ///< Per-core CPU statistics
    pico_rtos_memory_stats_t memory_stats;      ///< Memory statistics
    pico_rtos_system_health_t system_health;    ///< System health summary
    
    // Memory leak detection
    #if PICO_RTOS_HEALTH_ENABLE_LEAK_DETECTION
    pico_rtos_memory_leak_entry_t leak_entries[256]; ///< Memory leak tracking
    uint32_t leak_entry_count;                  ///< Number of tracked allocations
    uint32_t potential_leaks;                   ///< Number of potential leaks
    #endif
    
    critical_section_t cs;                      ///< Global critical section
};

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the health monitoring system
 * 
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_health_init(void);

/**
 * @brief Enable or disable health monitoring
 * 
 * @param enabled true to enable monitoring, false to disable
 */
void pico_rtos_health_set_enabled(bool enabled);

/**
 * @brief Check if health monitoring is enabled
 * 
 * @return true if enabled, false otherwise
 */
bool pico_rtos_health_is_enabled(void);

/**
 * @brief Set health monitoring sample interval
 * 
 * @param interval_ms Sampling interval in milliseconds
 */
void pico_rtos_health_set_sample_interval(uint32_t interval_ms);

/**
 * @brief Set alert callback function
 * 
 * @param callback Callback function to call when alerts are triggered
 * @param user_data User data to pass to callback
 */
void pico_rtos_health_set_alert_callback(pico_rtos_health_alert_callback_t callback, 
                                        void *user_data);

// =============================================================================
// METRIC MANAGEMENT API
// =============================================================================

/**
 * @brief Register a health metric
 * 
 * @param type Metric type
 * @param name Metric name
 * @param description Metric description
 * @param units Metric units
 * @param warning_threshold Warning threshold (0 to disable)
 * @param critical_threshold Critical threshold (0 to disable)
 * @return Metric ID, or 0 if registration failed
 */
uint32_t pico_rtos_health_register_metric(pico_rtos_health_metric_type_t type,
                                         const char *name,
                                         const char *description,
                                         const char *units,
                                         uint32_t warning_threshold,
                                         uint32_t critical_threshold);

/**
 * @brief Register a custom metric with callback
 * 
 * @param name Metric name
 * @param description Metric description
 * @param units Metric units
 * @param callback Callback function to collect metric value
 * @param user_data User data for callback
 * @param warning_threshold Warning threshold (0 to disable)
 * @param critical_threshold Critical threshold (0 to disable)
 * @return Metric ID, or 0 if registration failed
 */
uint32_t pico_rtos_health_register_custom_metric(const char *name,
                                                const char *description,
                                                const char *units,
                                                pico_rtos_health_metric_callback_t callback,
                                                void *user_data,
                                                uint32_t warning_threshold,
                                                uint32_t critical_threshold);

/**
 * @brief Unregister a health metric
 * 
 * @param metric_id Metric ID to unregister
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_unregister_metric(uint32_t metric_id);

/**
 * @brief Update a metric value
 * 
 * @param metric_id Metric ID
 * @param value New metric value
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_update_metric(uint32_t metric_id, uint32_t value);

/**
 * @brief Get metric information
 * 
 * @param metric_id Metric ID
 * @param metric Pointer to store metric information
 * @return true if metric found, false otherwise
 */
bool pico_rtos_health_get_metric(uint32_t metric_id, pico_rtos_health_metric_t **metric);

// =============================================================================
// SYSTEM STATISTICS API
// =============================================================================

/**
 * @brief Get CPU usage statistics
 * 
 * @param core_id Core ID (0 or 1 for RP2040)
 * @param stats Pointer to store CPU statistics
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_get_cpu_stats(uint32_t core_id, pico_rtos_cpu_stats_t *stats);

/**
 * @brief Get memory usage statistics
 * 
 * @param stats Pointer to store memory statistics
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_get_memory_stats(pico_rtos_memory_stats_t *stats);

/**
 * @brief Get task performance statistics
 * 
 * @param task Task to get statistics for (NULL for current task)
 * @param stats Pointer to store task statistics
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_get_task_stats(pico_rtos_task_t *task, pico_rtos_task_stats_t *stats);

/**
 * @brief Get system-wide health summary
 * 
 * @param health Pointer to store system health information
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_get_system_health(pico_rtos_system_health_t *health);

/**
 * @brief Get list of all tasks with statistics
 * 
 * @param task_stats Array to store task statistics
 * @param max_tasks Maximum number of tasks to return
 * @param actual_count Pointer to store actual number of tasks returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_get_all_task_stats(pico_rtos_task_stats_t *task_stats,
                                        uint32_t max_tasks,
                                        uint32_t *actual_count);

// =============================================================================
// ALERT MANAGEMENT API
// =============================================================================

/**
 * @brief Get list of active alerts
 * 
 * @param alerts Array to store alert information
 * @param max_alerts Maximum number of alerts to return
 * @param actual_count Pointer to store actual number of alerts returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_get_active_alerts(pico_rtos_health_alert_t *alerts,
                                       uint32_t max_alerts,
                                       uint32_t *actual_count);

/**
 * @brief Acknowledge an alert
 * 
 * @param alert_id Alert ID to acknowledge
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_acknowledge_alert(uint32_t alert_id);

/**
 * @brief Clear all acknowledged alerts
 * 
 * @return Number of alerts cleared
 */
uint32_t pico_rtos_health_clear_acknowledged_alerts(void);

// =============================================================================
// MEMORY LEAK DETECTION API
// =============================================================================

#if PICO_RTOS_HEALTH_ENABLE_LEAK_DETECTION

/**
 * @brief Track memory allocation for leak detection
 * 
 * @param address Allocated memory address
 * @param size Allocation size
 * @param file Source file name
 * @param line Source line number
 * @param function Source function name
 */
void pico_rtos_health_track_allocation(void *address, size_t size,
                                      const char *file, int line,
                                      const char *function);

/**
 * @brief Track memory deallocation for leak detection
 * 
 * @param address Deallocated memory address
 */
void pico_rtos_health_track_deallocation(void *address);

/**
 * @brief Perform memory leak detection scan
 * 
 * @return Number of potential leaks detected
 */
uint32_t pico_rtos_health_detect_memory_leaks(void);

/**
 * @brief Get memory leak report
 * 
 * @param leaks Array to store leak information
 * @param max_leaks Maximum number of leaks to return
 * @param actual_count Pointer to store actual number of leaks returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_health_get_memory_leaks(pico_rtos_memory_leak_entry_t *leaks,
                                      uint32_t max_leaks,
                                      uint32_t *actual_count);

#endif // PICO_RTOS_HEALTH_ENABLE_LEAK_DETECTION

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get string representation of health state
 * 
 * @param state Health state
 * @return String representation
 */
const char *pico_rtos_health_get_state_string(pico_rtos_health_state_t state);

/**
 * @brief Get string representation of metric type
 * 
 * @param type Metric type
 * @return String representation
 */
const char *pico_rtos_health_get_metric_type_string(pico_rtos_health_metric_type_t type);

/**
 * @brief Get string representation of alert level
 * 
 * @param level Alert level
 * @return String representation
 */
const char *pico_rtos_health_get_alert_level_string(pico_rtos_health_alert_level_t level);

/**
 * @brief Print system health summary
 */
void pico_rtos_health_print_summary(void);

/**
 * @brief Print detailed health report
 */
void pico_rtos_health_print_detailed_report(void);

// =============================================================================
// INTERNAL FUNCTIONS (for system use)
// =============================================================================

/**
 * @brief Perform periodic health monitoring (called by system)
 * 
 * This function is called periodically by the system to collect health metrics.
 * It should not be called directly by user code.
 */
void pico_rtos_health_periodic_update(void);

/**
 * @brief Update task statistics (called by scheduler)
 * 
 * @param task Task to update statistics for
 * @param runtime_us Runtime in microseconds
 */
void pico_rtos_health_update_task_runtime(pico_rtos_task_t *task, uint64_t runtime_us);

#endif // PICO_RTOS_HEALTH_H