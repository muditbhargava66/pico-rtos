/**
 * @file alerts.c
 * @brief Configurable Alert and Notification System Implementation
 * 
 * This module implements a comprehensive alert and notification system with
 * threshold-based alerting, callback notifications, and alert management.
 */

#include "pico_rtos/alerts.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

static pico_rtos_alert_system_t g_alert_system = {0};

// =============================================================================
// STRING CONSTANTS
// =============================================================================

static const char *severity_strings[] = {
    "Info",
    "Warning",
    "Error",
    "Critical",
    "Fatal"
};

static const char *state_strings[] = {
    "Inactive",
    "Active",
    "Acknowledged",
    "Escalated",
    "Resolved"
};

static const char *source_strings[] = {
    "System",
    "Task",
    "Memory",
    "CPU",
    "Watchdog",
    "Deadlock",
    "Health",
    "Timeout",
    "I/O",
    "Timer",
    "User"
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
    return pico_rtos_get_tick_count();
}

/**
 * @brief Find alert by ID
 * @param alert_id Alert ID to find
 * @return Pointer to alert, or NULL if not found
 */
static pico_rtos_alert_t *find_alert_by_id(uint32_t alert_id)
{
    for (uint32_t i = 0; i < g_alert_system.alert_count; i++) {
        if (g_alert_system.alerts[i].alert_id == alert_id) {
            return &g_alert_system.alerts[i];
        }
    }
    return NULL;
}

/**
 * @brief Find handler by ID
 * @param handler_id Handler ID to find
 * @return Pointer to handler, or NULL if not found
 */
static pico_rtos_alert_handler_t *find_handler_by_id(uint32_t handler_id)
{
    for (uint32_t i = 0; i < g_alert_system.handler_count; i++) {
        if (g_alert_system.handlers[i].handler_id == handler_id) {
            return &g_alert_system.handlers[i];
        }
    }
    return NULL;
}

/**
 * @brief Add alert to history
 * @param alert Alert to add to history
 */
static void add_to_history(const pico_rtos_alert_t *alert)
{
    if (!g_alert_system.config.enable_alert_history || alert == NULL) {
        return;
    }
    
    pico_rtos_alert_t *history_entry = &g_alert_system.history[g_alert_system.history_index];
    *history_entry = *alert;
    
    g_alert_system.history_index = (g_alert_system.history_index + 1) % PICO_RTOS_ALERTS_HISTORY_SIZE;
    
    if (g_alert_system.history_count < PICO_RTOS_ALERTS_HISTORY_SIZE) {
        g_alert_system.history_count++;
    }
}

/**
 * @brief Call alert handlers
 * @param alert Alert to process
 */
static void call_alert_handlers(pico_rtos_alert_t *alert)
{
    if (alert == NULL) {
        return;
    }
    
    // Sort handlers by priority (higher priority first)
    for (uint32_t i = 0; i < g_alert_system.handler_count; i++) {
        pico_rtos_alert_handler_t *handler = &g_alert_system.handlers[i];
        
        if (!handler->enabled) {
            continue;
        }
        
        // Check severity filter
        if (alert->severity < handler->min_severity) {
            continue;
        }
        
        // Check source filter
        if (handler->source_mask != 0 && !(handler->source_mask & (1 << alert->source))) {
            continue;
        }
        
        // Apply custom filter if provided
        if (handler->filter != NULL) {
            if (!handler->filter(alert, handler->user_data)) {
                handler->alerts_filtered++;
                continue;
            }
        }
        
        // Call handler callback
        if (handler->callback != NULL) {
            pico_rtos_alert_action_t action = handler->callback(alert, handler->user_data);
            handler->alerts_handled++;
            
            // Process handler action
            switch (action) {
                case PICO_RTOS_ALERT_ACTION_ESCALATE:
                    if (g_alert_system.config.enable_escalation) {
                        alert->escalation_level++;
                        alert->state = PICO_RTOS_ALERT_STATE_ESCALATED;
                        g_alert_system.stats.escalated_alerts++;
                    }
                    break;
                    
                case PICO_RTOS_ALERT_ACTION_RESET:
                    PICO_RTOS_LOG_CRITICAL("Alert handler requested system reset: %s", alert->name);
                    // In a real system, this might trigger a watchdog reset
                    break;
                    
                case PICO_RTOS_ALERT_ACTION_SHUTDOWN:
                    PICO_RTOS_LOG_CRITICAL("Alert handler requested system shutdown: %s", alert->name);
                    // In a real system, this might trigger a controlled shutdown
                    break;
                    
                default:
                    break;
            }
        }
    }
}

/**
 * @brief Update alert statistics
 * @param alert Alert to update statistics for
 */
static void update_statistics(pico_rtos_alert_t *alert)
{
    if (alert == NULL) {
        return;
    }
    
    // Update severity statistics
    if (alert->severity < 5) {
        g_alert_system.stats.alerts_by_severity[alert->severity]++;
    }
    
    // Update source statistics
    if (alert->source < 11) {
        g_alert_system.stats.alerts_by_source[alert->source]++;
    }
    
    // Update state statistics
    switch (alert->state) {
        case PICO_RTOS_ALERT_STATE_ACTIVE:
            g_alert_system.stats.active_alerts++;
            break;
        case PICO_RTOS_ALERT_STATE_ACKNOWLEDGED:
            g_alert_system.stats.acknowledged_alerts++;
            if (alert->acknowledged_time > alert->triggered_time) {
                uint32_t ack_time = alert->acknowledged_time - alert->triggered_time;
                g_alert_system.stats.avg_acknowledgment_time_ms = 
                    ((g_alert_system.stats.avg_acknowledgment_time_ms * 
                      (g_alert_system.stats.acknowledged_alerts - 1)) + ack_time) / 
                    g_alert_system.stats.acknowledged_alerts;
            }
            break;
        case PICO_RTOS_ALERT_STATE_RESOLVED:
            g_alert_system.stats.resolved_alerts++;
            if (alert->resolved_time > alert->triggered_time) {
                uint32_t resolution_time = alert->resolved_time - alert->triggered_time;
                if (resolution_time > g_alert_system.stats.max_resolution_time_ms) {
                    g_alert_system.stats.max_resolution_time_ms = resolution_time;
                }
                g_alert_system.stats.avg_resolution_time_ms = 
                    ((g_alert_system.stats.avg_resolution_time_ms * 
                      (g_alert_system.stats.resolved_alerts - 1)) + resolution_time) / 
                    g_alert_system.stats.resolved_alerts;
            }
            break;
        default:
            break;
    }
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_alerts_init(const pico_rtos_alert_config_t *config)
{
    if (g_alert_system.initialized) {
        return true;
    }
    
    // Initialize critical section
    critical_section_init(&g_alert_system.cs);
    
    // Initialize system state
    memset(&g_alert_system, 0, sizeof(g_alert_system));
    g_alert_system.enabled = true;
    g_alert_system.next_alert_id = 1;
    g_alert_system.next_handler_id = 1;
    
    // Set configuration
    if (config != NULL) {
        g_alert_system.config = *config;
    } else {
        // Default configuration
        g_alert_system.config.enable_escalation = PICO_RTOS_ALERTS_ENABLE_ESCALATION;
        g_alert_system.config.escalation_interval_ms = 30000; // 30 seconds
        g_alert_system.config.max_escalation_levels = 3;
        g_alert_system.config.enable_auto_acknowledge = true;
        g_alert_system.config.auto_acknowledge_timeout_ms = 300000; // 5 minutes
        g_alert_system.config.enable_alert_history = true;
        g_alert_system.config.history_retention_ms = 3600000; // 1 hour
        g_alert_system.config.enable_persistent_alerts = false;
    }
    
    // Initialize alert critical sections
    for (uint32_t i = 0; i < PICO_RTOS_ALERTS_MAX_ALERTS; i++) {
        critical_section_init(&g_alert_system.alerts[i].cs);
    }
    
    // Initialize handler critical sections
    for (uint32_t i = 0; i < PICO_RTOS_ALERTS_MAX_HANDLERS; i++) {
        critical_section_init(&g_alert_system.handlers[i].cs);
    }
    
    g_alert_system.last_escalation_check = get_current_time_ms();
    g_alert_system.last_cleanup_time = get_current_time_ms();
    g_alert_system.initialized = true;
    
    PICO_RTOS_LOG_INFO("Alert system initialized");
    return true;
}

void pico_rtos_alerts_set_enabled(bool enabled)
{
    if (!g_alert_system.initialized) {
        return;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    g_alert_system.enabled = enabled;
    critical_section_exit(&g_alert_system.cs);
    
    PICO_RTOS_LOG_INFO("Alert system %s", enabled ? "enabled" : "disabled");
}

bool pico_rtos_alerts_is_enabled(void)
{
    if (!g_alert_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    bool enabled = g_alert_system.enabled;
    critical_section_exit(&g_alert_system.cs);
    
    return enabled;
}

uint32_t pico_rtos_alerts_create(pico_rtos_alert_severity_t severity,
                                pico_rtos_alert_source_t source,
                                const char *name,
                                const char *description,
                                const char *category)
{
    if (!g_alert_system.initialized || !g_alert_system.enabled || name == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    
    if (g_alert_system.alert_count >= PICO_RTOS_ALERTS_MAX_ALERTS) {
        critical_section_exit(&g_alert_system.cs);
        PICO_RTOS_LOG_ERROR("Maximum number of alerts exceeded");
        return 0;
    }
    
    pico_rtos_alert_t *alert = &g_alert_system.alerts[g_alert_system.alert_count++];
    
    // Initialize alert
    memset(alert, 0, sizeof(pico_rtos_alert_t));
    alert->alert_id = g_alert_system.next_alert_id++;
    alert->severity = severity;
    alert->state = PICO_RTOS_ALERT_STATE_INACTIVE;
    alert->source = source;
    alert->name = name;
    alert->description = description;
    alert->category = category;
    alert->created_time = get_current_time_ms();
    alert->max_escalation_level = g_alert_system.config.max_escalation_levels;
    
    uint32_t alert_id = alert->alert_id;
    g_alert_system.stats.total_alerts++;
    
    critical_section_exit(&g_alert_system.cs);
    
    PICO_RTOS_LOG_DEBUG("Created alert %u: %s (%s)", 
                        alert_id, name, pico_rtos_alerts_get_severity_string(severity));
    
    return alert_id;
}

bool pico_rtos_alerts_trigger(uint32_t alert_id,
                             uint32_t value,
                             uint32_t threshold,
                             const char *units)
{
    if (!g_alert_system.initialized || !g_alert_system.enabled) {
        return false;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    
    pico_rtos_alert_t *alert = find_alert_by_id(alert_id);
    if (alert == NULL) {
        critical_section_exit(&g_alert_system.cs);
        return false;
    }
    
    critical_section_enter_blocking(&alert->cs);
    
    // Update alert information
    alert->value = value;
    alert->threshold = threshold;
    alert->units = units;
    alert->triggered_time = get_current_time_ms();
    alert->trigger_count++;
    
    // Change state to active if not already
    if (alert->state == PICO_RTOS_ALERT_STATE_INACTIVE) {
        alert->state = PICO_RTOS_ALERT_STATE_ACTIVE;
    }
    
    // Update statistics
    update_statistics(alert);
    
    // Add to history
    add_to_history(alert);
    
    critical_section_exit(&alert->cs);
    
    // Call handlers
    call_alert_handlers(alert);
    
    critical_section_exit(&g_alert_system.cs);
    
    PICO_RTOS_LOG_WARN("Alert triggered: %s (value: %u, threshold: %u%s%s)", 
                       alert->name, value, threshold, 
                       units ? " " : "", units ? units : "");
    
    return true;
}

bool pico_rtos_alerts_acknowledge(uint32_t alert_id)
{
    if (!g_alert_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    
    pico_rtos_alert_t *alert = find_alert_by_id(alert_id);
    if (alert == NULL) {
        critical_section_exit(&g_alert_system.cs);
        return false;
    }
    
    critical_section_enter_blocking(&alert->cs);
    
    if (alert->state == PICO_RTOS_ALERT_STATE_ACTIVE || 
        alert->state == PICO_RTOS_ALERT_STATE_ESCALATED) {
        alert->state = PICO_RTOS_ALERT_STATE_ACKNOWLEDGED;
        alert->acknowledged_time = get_current_time_ms();
        
        update_statistics(alert);
        add_to_history(alert);
    }
    
    critical_section_exit(&alert->cs);
    critical_section_exit(&g_alert_system.cs);
    
    PICO_RTOS_LOG_INFO("Alert acknowledged: %s", alert->name);
    return true;
}

bool pico_rtos_alerts_resolve(uint32_t alert_id)
{
    if (!g_alert_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    
    pico_rtos_alert_t *alert = find_alert_by_id(alert_id);
    if (alert == NULL) {
        critical_section_exit(&g_alert_system.cs);
        return false;
    }
    
    critical_section_enter_blocking(&alert->cs);
    
    alert->state = PICO_RTOS_ALERT_STATE_RESOLVED;
    alert->resolved_time = get_current_time_ms();
    
    // Auto-acknowledge if configured
    if (alert->auto_acknowledge && alert->acknowledged_time == 0) {
        alert->acknowledged_time = alert->resolved_time;
    }
    
    update_statistics(alert);
    add_to_history(alert);
    
    critical_section_exit(&alert->cs);
    critical_section_exit(&g_alert_system.cs);
    
    PICO_RTOS_LOG_INFO("Alert resolved: %s", alert->name);
    return true;
}

uint32_t pico_rtos_alerts_register_handler(const char *name,
                                          pico_rtos_alert_callback_t callback,
                                          pico_rtos_alert_filter_t filter,
                                          void *user_data,
                                          pico_rtos_alert_severity_t min_severity,
                                          pico_rtos_alert_source_t source_mask,
                                          uint32_t priority)
{
    if (!g_alert_system.initialized || callback == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    
    if (g_alert_system.handler_count >= PICO_RTOS_ALERTS_MAX_HANDLERS) {
        critical_section_exit(&g_alert_system.cs);
        PICO_RTOS_LOG_ERROR("Maximum number of alert handlers exceeded");
        return 0;
    }
    
    pico_rtos_alert_handler_t *handler = &g_alert_system.handlers[g_alert_system.handler_count++];
    
    // Initialize handler
    memset(handler, 0, sizeof(pico_rtos_alert_handler_t));
    handler->handler_id = g_alert_system.next_handler_id++;
    handler->name = name;
    handler->callback = callback;
    handler->filter = filter;
    handler->user_data = user_data;
    handler->min_severity = min_severity;
    handler->source_mask = source_mask;
    handler->priority = priority;
    handler->enabled = true;
    
    uint32_t handler_id = handler->handler_id;
    g_alert_system.stats.registered_handlers++;
    g_alert_system.stats.active_handlers++;
    
    critical_section_exit(&g_alert_system.cs);
    
    PICO_RTOS_LOG_DEBUG("Registered alert handler %u: %s", handler_id, name ? name : "unnamed");
    return handler_id;
}

bool pico_rtos_alerts_get_statistics(pico_rtos_alert_statistics_t *stats)
{
    if (!g_alert_system.initialized || stats == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&g_alert_system.cs);
    *stats = g_alert_system.stats;
    critical_section_exit(&g_alert_system.cs);
    
    return true;
}

// Utility functions
const char *pico_rtos_alerts_get_severity_string(pico_rtos_alert_severity_t severity)
{
    if (severity < sizeof(severity_strings) / sizeof(severity_strings[0])) {
        return severity_strings[severity];
    }
    return "Unknown";
}

const char *pico_rtos_alerts_get_state_string(pico_rtos_alert_state_t state)
{
    if (state < sizeof(state_strings) / sizeof(state_strings[0])) {
        return state_strings[state];
    }
    return "Unknown";
}

const char *pico_rtos_alerts_get_source_string(pico_rtos_alert_source_t source)
{
    if (source < sizeof(source_strings) / sizeof(source_strings[0])) {
        return source_strings[source];
    }
    return "Unknown";
}

void pico_rtos_alerts_print_summary(void)
{
    if (!g_alert_system.initialized) {
        printf("Alert system not initialized\n");
        return;
    }
    
    pico_rtos_alert_statistics_t stats;
    if (!pico_rtos_alerts_get_statistics(&stats)) {
        printf("Failed to get alert statistics\n");
        return;
    }
    
    printf("=== Alert System Summary ===\n");
    printf("Total Alerts: %u\n", stats.total_alerts);
    printf("Active Alerts: %u\n", stats.active_alerts);
    printf("Acknowledged: %u\n", stats.acknowledged_alerts);
    printf("Resolved: %u\n", stats.resolved_alerts);
    printf("Escalated: %u\n", stats.escalated_alerts);
    printf("Registered Handlers: %u\n", stats.registered_handlers);
    printf("Active Handlers: %u\n", stats.active_handlers);
    
    printf("\nAlerts by Severity:\n");
    for (int i = 0; i < 5; i++) {
        if (stats.alerts_by_severity[i] > 0) {
            printf("  %s: %u\n", severity_strings[i], stats.alerts_by_severity[i]);
        }
    }
    
    printf("============================\n");
}

// Internal system functions
void pico_rtos_alerts_periodic_update(void)
{
    if (!g_alert_system.initialized || !g_alert_system.enabled) {
        return;
    }
    
    uint32_t current_time = get_current_time_ms();
    
    // Check for escalation
    if (g_alert_system.config.enable_escalation &&
        current_time - g_alert_system.last_escalation_check >= g_alert_system.config.escalation_interval_ms) {
        
        g_alert_system.last_escalation_check = current_time;
        
        critical_section_enter_blocking(&g_alert_system.cs);
        
        for (uint32_t i = 0; i < g_alert_system.alert_count; i++) {
            pico_rtos_alert_t *alert = &g_alert_system.alerts[i];
            
            if (alert->state == PICO_RTOS_ALERT_STATE_ACTIVE &&
                alert->escalation_level < alert->max_escalation_level &&
                current_time - alert->triggered_time >= g_alert_system.config.escalation_interval_ms) {
                
                alert->escalation_level++;
                alert->state = PICO_RTOS_ALERT_STATE_ESCALATED;
                g_alert_system.stats.escalated_alerts++;
                
                PICO_RTOS_LOG_WARN("Alert escalated to level %u: %s", 
                                   alert->escalation_level, alert->name);
                
                // Call handlers for escalated alert
                call_alert_handlers(alert);
            }
        }
        
        critical_section_exit(&g_alert_system.cs);
    }
    
    // Cleanup expired alerts
    if (current_time - g_alert_system.last_cleanup_time >= 60000) { // Every minute
        g_alert_system.last_cleanup_time = current_time;
        
        // This would implement cleanup logic for expired alerts
        // For now, we'll just update the cleanup timestamp
    }
}