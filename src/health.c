/**
 * @file health.c
 * @brief Comprehensive System Health Monitoring Implementation
 * 
 * This module implements comprehensive system health monitoring capabilities,
 * tracking CPU usage, memory usage, task performance, and system metrics.
 */

#include "pico_rtos/health.h"
#include "pico_rtos/task.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include "hardware/timer.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

static pico_rtos_health_monitor_t g_health_monitor = {0};

// =============================================================================
// STRING CONSTANTS
// =============================================================================

static const char *health_state_strings[] = {
    "Healthy",
    "Warning",
    "Critical",
    "Failure"
};

static const char *metric_type_strings[] = {
    "CPU Usage",
    "Memory Usage",
    "Stack Usage",
    "Task Runtime",
    "Interrupt Latency",
    "Context Switches",
    "Queue Usage",
    "Mutex Contention",
    "Custom"
};

static const char *alert_level_strings[] = {
    "Info",
    "Warning",
    "Error",
    "Critical"
};

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds
 */
static inline uint32_t get_current_time_ms(void)
{
    return (uint32_t)(time_us_64() / 1000);
}

/**
 * @brief Find metric by ID
 * @param metric_id Metric ID to find
 * @return Pointer to metric, or NULL if not found
 */
static pico_rtos_health_metric_t *find_metric_by_id(uint32_t metric_id)
{
    for (uint32_t i = 0; i < g_health_monitor.metric_count; i++) {
        if (g_health_monitor.metrics[i].metric_id == metric_id &&
            g_health_monitor.metrics[i].active) {
            return &g_health_monitor.metrics[i];
        }
    }
    return NULL;
}

/**
 * @brief Add value to metric history
 * @param metric Metric to update
 * @param value Value to add
 */
static void add_to_metric_history(pico_rtos_health_metric_t *metric, uint32_t value)
{
    metric->history[metric->history_index] = value;
    metric->history_index = (metric->history_index + 1) % PICO_RTOS_HEALTH_HISTORY_SIZE;
    
    if (!metric->history_full && metric->history_index == 0) {
        metric->history_full = true;
    }
}

/**
 * @brief Calculate metric average from history
 * @param metric Metric to calculate average for
 * @return Average value
 */
static uint32_t calculate_metric_average(pico_rtos_health_metric_t *metric)
{
    uint32_t sum = 0;
    uint32_t count = metric->history_full ? PICO_RTOS_HEALTH_HISTORY_SIZE : metric->history_index;
    
    if (count == 0) {
        return metric->current_value;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        sum += metric->history[i];
    }
    
    return sum / count;
}

/**
 * @brief Check metric thresholds and generate alerts
 * @param metric Metric to check
 */
static void check_metric_thresholds(pico_rtos_health_metric_t *metric)
{
    if (!metric->threshold_enabled) {
        return;
    }
    
    pico_rtos_health_alert_level_t alert_level = PICO_RTOS_HEALTH_ALERT_INFO;
    bool trigger_alert = false;
    
    if (metric->critical_threshold > 0 && metric->current_value >= metric->critical_threshold) {
        alert_level = PICO_RTOS_HEALTH_ALERT_CRITICAL;
        trigger_alert = true;
    } else if (metric->warning_threshold > 0 && metric->current_value >= metric->warning_threshold) {
        alert_level = PICO_RTOS_HEALTH_ALERT_WARNING;
        trigger_alert = true;
    }
    
    if (trigger_alert && g_health_monitor.alert_count < PICO_RTOS_HEALTH_MAX_ALERTS) {
        pico_rtos_health_alert_t *alert = &g_health_monitor.alerts[g_health_monitor.alert_count++];
        
        alert->alert_id = g_health_monitor.next_alert_id++;
        alert->level = alert_level;
        alert->metric_type = metric->type;
        alert->metric_id = metric->metric_id;
        alert->message = metric->name;
        alert->description = metric->description;
        alert->value = metric->current_value;
        alert->threshold = (alert_level == PICO_RTOS_HEALTH_ALERT_CRITICAL) ? 
                         metric->critical_threshold : metric->warning_threshold;
        alert->timestamp = get_current_time_ms();
        alert->acknowledged = false;
        alert->active = true;
        
        // Call alert callback if registered
        if (g_health_monitor.alert_callback != NULL) {
            g_health_monitor.alert_callback(alert, g_health_monitor.alert_callback_data);
        }
        
        PICO_RTOS_LOG_WARN("Health alert: %s = %u (threshold: %u)", 
                           metric->name, metric->current_value, alert->threshold);
    }
}

/**
 * @brief Update CPU statistics
 * @param core_id Core ID to update
 */
static void update_cpu_stats(uint32_t core_id)
{
    if (core_id >= 2) return; // RP2040 has 2 cores
    
    pico_rtos_cpu_stats_t *stats = &g_health_monitor.cpu_stats[core_id];
    
    // Get current system statistics
    uint32_t current_time = get_current_time_ms();
    static uint32_t last_update_time[2] = {0, 0};
    
    if (last_update_time[core_id] == 0) {
        last_update_time[core_id] = current_time;
        return;
    }
    
    uint32_t time_delta = current_time - last_update_time[core_id];
    if (time_delta == 0) return;
    
    // Update basic stats
    stats->core_id = core_id;
    stats->total_runtime_us += time_delta * 1000;
    
    // Get idle counter (this would need to be implemented in the scheduler)
    uint32_t idle_counter = pico_rtos_get_idle_counter();
    static uint32_t last_idle_counter[2] = {0, 0};
    
    uint32_t idle_delta = idle_counter - last_idle_counter[core_id];
    stats->idle_time_us += idle_delta;
    stats->active_time_us = stats->total_runtime_us - stats->idle_time_us;
    
    // Calculate CPU usage percentage
    if (stats->total_runtime_us > 0) {
        stats->usage_percent = (stats->active_time_us * 100) / stats->total_runtime_us;
    }
    
    last_idle_counter[core_id] = idle_counter;
    last_update_time[core_id] = current_time;
}

/**
 * @brief Update memory statistics
 */
static void update_memory_stats(void)
{
    pico_rtos_memory_stats_t *stats = &g_health_monitor.memory_stats;
    
    // Get current memory statistics from RTOS
    uint32_t current_usage, peak_usage, allocations;
    pico_rtos_get_memory_stats(&current_usage, &peak_usage, &allocations);
    
    stats->used_heap_size = current_usage;
    stats->peak_heap_usage = peak_usage;
    stats->total_allocations = allocations;
    stats->current_allocations = allocations - stats->total_deallocations;
    
    // Calculate derived statistics
    if (stats->total_heap_size > 0) {
        stats->free_heap_size = stats->total_heap_size - stats->used_heap_size;
        
        // Simple fragmentation estimation
        if (stats->largest_free_block > 0) {
            stats->heap_fragmentation_percent = 
                ((stats->free_heap_size - stats->largest_free_block) * 100) / stats->free_heap_size;
        }
    }
}

/**
 * @brief Update system health summary
 */
static void update_system_health(void)
{
    pico_rtos_system_health_t *health = &g_health_monitor.system_health;
    
    // Update basic system information
    health->uptime_ms = get_current_time_ms();
    health->tick_count = pico_rtos_get_tick_count();
    health->tick_rate_hz = pico_rtos_get_tick_rate_hz();
    
    // Get system statistics
    pico_rtos_system_stats_t sys_stats;
    pico_rtos_get_system_stats(&sys_stats);
    
    health->total_tasks = sys_stats.total_tasks;
    health->ready_tasks = sys_stats.ready_tasks;
    health->blocked_tasks = sys_stats.blocked_tasks;
    health->suspended_tasks = sys_stats.suspended_tasks;
    health->terminated_tasks = sys_stats.terminated_tasks;
    
    // Determine overall health state based on various factors
    health->overall_health = PICO_RTOS_HEALTH_STATE_HEALTHY;
    
    // Check CPU usage
    for (int i = 0; i < 2; i++) {
        if (g_health_monitor.cpu_stats[i].usage_percent > 90) {
            health->overall_health = PICO_RTOS_HEALTH_STATE_CRITICAL;
            break;
        } else if (g_health_monitor.cpu_stats[i].usage_percent > 75) {
            health->overall_health = PICO_RTOS_HEALTH_STATE_WARNING;
        }
    }
    
    // Check memory usage
    if (g_health_monitor.memory_stats.total_heap_size > 0) {
        uint32_t memory_usage_percent = 
            (g_health_monitor.memory_stats.used_heap_size * 100) / 
            g_health_monitor.memory_stats.total_heap_size;
        
        if (memory_usage_percent > 90) {
            health->overall_health = PICO_RTOS_HEALTH_STATE_CRITICAL;
        } else if (memory_usage_percent > 75 && 
                   health->overall_health == PICO_RTOS_HEALTH_STATE_HEALTHY) {
            health->overall_health = PICO_RTOS_HEALTH_STATE_WARNING;
        }
    }
    
    // Check for active critical alerts
    for (uint32_t i = 0; i < g_health_monitor.alert_count; i++) {
        if (g_health_monitor.alerts[i].active && 
            g_health_monitor.alerts[i].level == PICO_RTOS_HEALTH_ALERT_CRITICAL) {
            health->overall_health = PICO_RTOS_HEALTH_STATE_CRITICAL;
            break;
        }
    }
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_health_init(void)
{
    if (g_health_monitor.initialized) {
        return true;
    }
    
    // Initialize critical section
    critical_section_init(&g_health_monitor.cs);
    
    // Initialize monitor state
    memset(&g_health_monitor, 0, sizeof(g_health_monitor));
    g_health_monitor.enabled = true;
    g_health_monitor.sample_interval_ms = PICO_RTOS_HEALTH_SAMPLE_INTERVAL_MS;
    g_health_monitor.next_metric_id = 1;
    g_health_monitor.next_alert_id = 1;
    g_health_monitor.last_sample_time = get_current_time_ms();
    
    // Initialize metric critical sections
    for (uint32_t i = 0; i < 32; i++) {
        critical_section_init(&g_health_monitor.metrics[i].cs);
    }
    
    // Initialize memory statistics with reasonable defaults
    g_health_monitor.memory_stats.total_heap_size = 256 * 1024; // 256KB default
    g_health_monitor.memory_stats.largest_free_block = g_health_monitor.memory_stats.total_heap_size;
    
    g_health_monitor.initialized = true;
    
    PICO_RTOS_LOG_INFO("System health monitoring initialized");
    return true;
}

void pico_rtos_health_set_enabled(bool enabled)
{
    if (!g_health_monitor.initialized) {
        return;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    g_health_monitor.enabled = enabled;
    critical_section_exit(&g_health_monitor.cs);
    
    PICO_RTOS_LOG_INFO("Health monitoring %s", enabled ? "enabled" : "disabled");
}

bool pico_rtos_health_is_enabled(void)
{
    if (!g_health_monitor.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    bool enabled = g_health_monitor.enabled;
    critical_section_exit(&g_health_monitor.cs);
    
    return enabled;
}

void pico_rtos_health_set_sample_interval(uint32_t interval_ms)
{
    if (!g_health_monitor.initialized) {
        return;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    g_health_monitor.sample_interval_ms = interval_ms;
    critical_section_exit(&g_health_monitor.cs);
}

void pico_rtos_health_set_alert_callback(pico_rtos_health_alert_callback_t callback, 
                                        void *user_data)
{
    if (!g_health_monitor.initialized) {
        return;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    g_health_monitor.alert_callback = callback;
    g_health_monitor.alert_callback_data = user_data;
    critical_section_exit(&g_health_monitor.cs);
}

uint32_t pico_rtos_health_register_metric(pico_rtos_health_metric_type_t type,
                                         const char *name,
                                         const char *description,
                                         const char *units,
                                         uint32_t warning_threshold,
                                         uint32_t critical_threshold)
{
    if (!g_health_monitor.initialized || name == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    
    if (g_health_monitor.metric_count >= 32) {
        critical_section_exit(&g_health_monitor.cs);
        PICO_RTOS_LOG_ERROR("Maximum number of health metrics exceeded");
        return 0;
    }
    
    pico_rtos_health_metric_t *metric = &g_health_monitor.metrics[g_health_monitor.metric_count++];
    
    // Initialize metric
    memset(metric, 0, sizeof(pico_rtos_health_metric_t));
    metric->metric_id = g_health_monitor.next_metric_id++;
    metric->type = type;
    metric->name = name;
    metric->description = description;
    metric->units = units;
    metric->warning_threshold = warning_threshold;
    metric->critical_threshold = critical_threshold;
    metric->threshold_enabled = (warning_threshold > 0 || critical_threshold > 0);
    metric->active = true;
    metric->last_update_time = get_current_time_ms();
    metric->min_value = UINT32_MAX;
    metric->max_value = 0;
    
    uint32_t metric_id = metric->metric_id;
    
    critical_section_exit(&g_health_monitor.cs);
    
    PICO_RTOS_LOG_DEBUG("Registered health metric %u: %s", metric_id, name);
    return metric_id;
}

bool pico_rtos_health_update_metric(uint32_t metric_id, uint32_t value)
{
    if (!g_health_monitor.initialized || !g_health_monitor.enabled) {
        return false;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    
    pico_rtos_health_metric_t *metric = find_metric_by_id(metric_id);
    if (metric == NULL) {
        critical_section_exit(&g_health_monitor.cs);
        return false;
    }
    
    critical_section_enter_blocking(&metric->cs);
    
    // Update metric values
    metric->current_value = value;
    metric->sample_count++;
    metric->last_update_time = get_current_time_ms();
    
    // Update min/max
    if (value < metric->min_value) {
        metric->min_value = value;
    }
    if (value > metric->max_value) {
        metric->max_value = value;
    }
    
    // Add to history
    add_to_metric_history(metric, value);
    
    // Calculate average
    metric->average_value = calculate_metric_average(metric);
    
    // Check thresholds
    check_metric_thresholds(metric);
    
    critical_section_exit(&metric->cs);
    critical_section_exit(&g_health_monitor.cs);
    
    return true;
}

bool pico_rtos_health_get_cpu_stats(uint32_t core_id, pico_rtos_cpu_stats_t *stats)
{
    if (!g_health_monitor.initialized || core_id >= 2 || stats == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    *stats = g_health_monitor.cpu_stats[core_id];
    critical_section_exit(&g_health_monitor.cs);
    
    return true;
}

bool pico_rtos_health_get_memory_stats(pico_rtos_memory_stats_t *stats)
{
    if (!g_health_monitor.initialized || stats == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    *stats = g_health_monitor.memory_stats;
    critical_section_exit(&g_health_monitor.cs);
    
    return true;
}

bool pico_rtos_health_get_system_health(pico_rtos_system_health_t *health)
{
    if (!g_health_monitor.initialized || health == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&g_health_monitor.cs);
    *health = g_health_monitor.system_health;
    critical_section_exit(&g_health_monitor.cs);
    
    return true;
}

// Utility functions
const char *pico_rtos_health_get_state_string(pico_rtos_health_state_t state)
{
    if (state < sizeof(health_state_strings) / sizeof(health_state_strings[0])) {
        return health_state_strings[state];
    }
    return "Unknown";
}

const char *pico_rtos_health_get_metric_type_string(pico_rtos_health_metric_type_t type)
{
    if (type < sizeof(metric_type_strings) / sizeof(metric_type_strings[0])) {
        return metric_type_strings[type];
    }
    return "Unknown";
}

const char *pico_rtos_health_get_alert_level_string(pico_rtos_health_alert_level_t level)
{
    if (level < sizeof(alert_level_strings) / sizeof(alert_level_strings[0])) {
        return alert_level_strings[level];
    }
    return "Unknown";
}

void pico_rtos_health_print_summary(void)
{
    if (!g_health_monitor.initialized) {
        printf("Health monitoring not initialized\n");
        return;
    }
    
    pico_rtos_system_health_t health;
    if (!pico_rtos_health_get_system_health(&health)) {
        printf("Failed to get system health\n");
        return;
    }
    
    printf("=== System Health Summary ===\n");
    printf("Overall Health: %s\n", pico_rtos_health_get_state_string(health.overall_health));
    printf("Uptime: %u ms\n", health.uptime_ms);
    printf("Tasks: %u total (%u ready, %u blocked, %u suspended)\n",
           health.total_tasks, health.ready_tasks, health.blocked_tasks, health.suspended_tasks);
    
    // CPU statistics
    for (int i = 0; i < 2; i++) {
        pico_rtos_cpu_stats_t cpu_stats;
        if (pico_rtos_health_get_cpu_stats(i, &cpu_stats)) {
            printf("CPU %d Usage: %u%%\n", i, cpu_stats.usage_percent);
        }
    }
    
    // Memory statistics
    pico_rtos_memory_stats_t mem_stats;
    if (pico_rtos_health_get_memory_stats(&mem_stats)) {
        printf("Memory Usage: %u/%u bytes (%u%% used)\n",
               mem_stats.used_heap_size, mem_stats.total_heap_size,
               mem_stats.total_heap_size > 0 ? 
               (mem_stats.used_heap_size * 100) / mem_stats.total_heap_size : 0);
    }
    
    printf("Active Alerts: %u\n", g_health_monitor.alert_count);
    printf("==============================\n");
}

// Internal system functions
void pico_rtos_health_periodic_update(void)
{
    if (!g_health_monitor.initialized || !g_health_monitor.enabled) {
        return;
    }
    
    uint32_t current_time = get_current_time_ms();
    
    // Check if it's time for periodic update
    if (current_time - g_health_monitor.last_sample_time < g_health_monitor.sample_interval_ms) {
        return;
    }
    
    g_health_monitor.last_sample_time = current_time;
    
    // Update CPU statistics for both cores
    update_cpu_stats(0);
    update_cpu_stats(1);
    
    // Update memory statistics
    update_memory_stats();
    
    // Update system health summary
    update_system_health();
    
    // Update custom metrics
    for (uint32_t i = 0; i < g_health_monitor.metric_count; i++) {
        pico_rtos_health_metric_t *metric = &g_health_monitor.metrics[i];
        
        if (metric->active && metric->callback != NULL) {
            uint32_t value = metric->callback(metric, metric->user_data);
            pico_rtos_health_update_metric(metric->metric_id, value);
        }
    }
}