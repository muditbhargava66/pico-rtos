#ifndef PICO_RTOS_ALERTS_H
#define PICO_RTOS_ALERTS_H

#include <stdint.h>
#include <stdbool.h>
#include "pico_rtos/config.h"
#include "pico_rtos/types.h"
#include "pico/critical_section.h"

/**
 * @file alerts.h
 * @brief Configurable Alert and Notification System for Pico-RTOS
 * 
 * This module provides a comprehensive alert and notification system with
 * threshold-based alerting, callback notifications, alert escalation,
 * and acknowledgment mechanisms for system monitoring and maintenance.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef PICO_RTOS_ALERTS_MAX_ALERTS
#define PICO_RTOS_ALERTS_MAX_ALERTS 32
#endif

#ifndef PICO_RTOS_ALERTS_MAX_HANDLERS
#define PICO_RTOS_ALERTS_MAX_HANDLERS 16
#endif

#ifndef PICO_RTOS_ALERTS_HISTORY_SIZE
#define PICO_RTOS_ALERTS_HISTORY_SIZE 64
#endif

#ifndef PICO_RTOS_ALERTS_ENABLE_ESCALATION
#define PICO_RTOS_ALERTS_ENABLE_ESCALATION 1
#endif

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

/**
 * @brief Alert severity levels
 */
typedef enum {
    PICO_RTOS_ALERT_SEVERITY_INFO = 0,
    PICO_RTOS_ALERT_SEVERITY_WARNING,
    PICO_RTOS_ALERT_SEVERITY_ERROR,
    PICO_RTOS_ALERT_SEVERITY_CRITICAL,
    PICO_RTOS_ALERT_SEVERITY_FATAL
} pico_rtos_alert_severity_t;

/**
 * @brief Alert states
 */
typedef enum {
    PICO_RTOS_ALERT_STATE_INACTIVE = 0,
    PICO_RTOS_ALERT_STATE_ACTIVE,
    PICO_RTOS_ALERT_STATE_ACKNOWLEDGED,
    PICO_RTOS_ALERT_STATE_ESCALATED,
    PICO_RTOS_ALERT_STATE_RESOLVED
} pico_rtos_alert_state_t;

/**
 * @brief Alert sources
 */
typedef enum {
    PICO_RTOS_ALERT_SOURCE_SYSTEM = 0,
    PICO_RTOS_ALERT_SOURCE_TASK,
    PICO_RTOS_ALERT_SOURCE_MEMORY,
    PICO_RTOS_ALERT_SOURCE_CPU,
    PICO_RTOS_ALERT_SOURCE_WATCHDOG,
    PICO_RTOS_ALERT_SOURCE_DEADLOCK,
    PICO_RTOS_ALERT_SOURCE_HEALTH,
    PICO_RTOS_ALERT_SOURCE_TIMEOUT,
    PICO_RTOS_ALERT_SOURCE_IO,
    PICO_RTOS_ALERT_SOURCE_TIMER,
    PICO_RTOS_ALERT_SOURCE_USER
} pico_rtos_alert_source_t;

/**
 * @brief Alert actions
 */
typedef enum {
    PICO_RTOS_ALERT_ACTION_NONE = 0,
    PICO_RTOS_ALERT_ACTION_LOG,
    PICO_RTOS_ALERT_ACTION_CALLBACK,
    PICO_RTOS_ALERT_ACTION_ESCALATE,
    PICO_RTOS_ALERT_ACTION_RESET,
    PICO_RTOS_ALERT_ACTION_SHUTDOWN
} pico_rtos_alert_action_t;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct pico_rtos_alert pico_rtos_alert_t;
typedef struct pico_rtos_alert_handler pico_rtos_alert_handler_t;
typedef struct pico_rtos_alert_system pico_rtos_alert_system_t;

// =============================================================================
// CALLBACK TYPES
// =============================================================================

/**
 * @brief Alert notification callback function type
 * 
 * Called when an alert is triggered, escalated, or resolved.
 * 
 * @param alert Pointer to alert information
 * @param user_data User-defined data
 * @return Action to take for this alert
 */
typedef pico_rtos_alert_action_t (*pico_rtos_alert_callback_t)(
    const pico_rtos_alert_t *alert, void *user_data);

/**
 * @brief Alert filter callback function type
 * 
 * Called to determine if an alert should be processed.
 * 
 * @param alert Pointer to alert information
 * @param user_data User-defined data
 * @return true to process alert, false to filter out
 */
typedef bool (*pico_rtos_alert_filter_t)(
    const pico_rtos_alert_t *alert, void *user_data);

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Alert structure
 */
struct pico_rtos_alert {
    uint32_t alert_id;                          ///< Unique alert identifier
    pico_rtos_alert_severity_t severity;        ///< Alert severity level
    pico_rtos_alert_state_t state;              ///< Current alert state
    pico_rtos_alert_source_t source;            ///< Alert source
    
    const char *name;                           ///< Alert name
    const char *description;                    ///< Detailed description
    const char *category;                       ///< Alert category
    
    uint32_t value;                             ///< Current value that triggered alert
    uint32_t threshold;                         ///< Threshold that was exceeded
    const char *units;                          ///< Units for value/threshold
    
    uint32_t created_time;                      ///< Alert creation timestamp
    uint32_t triggered_time;                    ///< Alert trigger timestamp
    uint32_t acknowledged_time;                 ///< Alert acknowledgment timestamp
    uint32_t resolved_time;                     ///< Alert resolution timestamp
    
    uint32_t trigger_count;                     ///< Number of times triggered
    uint32_t escalation_level;                  ///< Current escalation level
    uint32_t max_escalation_level;              ///< Maximum escalation level
    
    bool auto_acknowledge;                      ///< Auto-acknowledge when resolved
    bool persistent;                            ///< Alert persists across resets
    uint32_t timeout_ms;                        ///< Alert timeout (0 = no timeout)
    
    void *context_data;                         ///< Context-specific data
    size_t context_size;                        ///< Size of context data
    
    critical_section_t cs;                      ///< Critical section for thread safety
};

/**
 * @brief Alert handler structure
 */
struct pico_rtos_alert_handler {
    uint32_t handler_id;                        ///< Unique handler identifier
    const char *name;                           ///< Handler name
    pico_rtos_alert_callback_t callback;        ///< Notification callback
    pico_rtos_alert_filter_t filter;            ///< Alert filter (optional)
    void *user_data;                            ///< User data for callbacks
    
    pico_rtos_alert_severity_t min_severity;    ///< Minimum severity to handle
    pico_rtos_alert_source_t source_mask;       ///< Source mask (0 = all sources)
    uint32_t priority;                          ///< Handler priority
    
    bool enabled;                               ///< Handler is enabled
    uint32_t alerts_handled;                    ///< Number of alerts handled
    uint32_t alerts_filtered;                   ///< Number of alerts filtered
    
    critical_section_t cs;                      ///< Critical section for thread safety
};

/**
 * @brief Alert configuration structure
 */
typedef struct {
    bool enable_escalation;                     ///< Enable alert escalation
    uint32_t escalation_interval_ms;            ///< Escalation interval
    uint32_t max_escalation_levels;             ///< Maximum escalation levels
    bool enable_auto_acknowledge;               ///< Enable auto-acknowledgment
    uint32_t auto_acknowledge_timeout_ms;       ///< Auto-acknowledge timeout
    bool enable_alert_history;                 ///< Enable alert history
    uint32_t history_retention_ms;              ///< History retention time
    bool enable_persistent_alerts;              ///< Enable persistent alerts
} pico_rtos_alert_config_t;

/**
 * @brief Alert statistics structure
 */
typedef struct {
    uint32_t total_alerts;                      ///< Total alerts created
    uint32_t active_alerts;                     ///< Currently active alerts
    uint32_t acknowledged_alerts;               ///< Acknowledged alerts
    uint32_t resolved_alerts;                   ///< Resolved alerts
    uint32_t escalated_alerts;                  ///< Escalated alerts
    uint32_t filtered_alerts;                   ///< Filtered alerts
    uint32_t expired_alerts;                    ///< Expired alerts
    
    uint32_t alerts_by_severity[5];             ///< Alerts by severity level
    uint32_t alerts_by_source[11];              ///< Alerts by source
    
    uint32_t avg_resolution_time_ms;            ///< Average resolution time
    uint32_t max_resolution_time_ms;            ///< Maximum resolution time
    uint32_t avg_acknowledgment_time_ms;        ///< Average acknowledgment time
    
    uint32_t registered_handlers;               ///< Number of registered handlers
    uint32_t active_handlers;                   ///< Number of active handlers
} pico_rtos_alert_statistics_t;

/**
 * @brief Alert system main structure
 */
struct pico_rtos_alert_system {
    bool initialized;                           ///< System is initialized
    bool enabled;                               ///< Alert system is enabled
    pico_rtos_alert_config_t config;            ///< System configuration
    
    // Alert management
    pico_rtos_alert_t alerts[PICO_RTOS_ALERTS_MAX_ALERTS]; ///< Alert pool
    uint32_t alert_count;                       ///< Number of active alerts
    uint32_t next_alert_id;                     ///< Next alert ID to assign
    
    // Handler management
    pico_rtos_alert_handler_t handlers[PICO_RTOS_ALERTS_MAX_HANDLERS]; ///< Handler pool
    uint32_t handler_count;                     ///< Number of registered handlers
    uint32_t next_handler_id;                   ///< Next handler ID to assign
    
    // Alert history
    pico_rtos_alert_t history[PICO_RTOS_ALERTS_HISTORY_SIZE]; ///< Alert history
    uint32_t history_count;                     ///< Number of history entries
    uint32_t history_index;                     ///< Current history index
    
    // Statistics
    pico_rtos_alert_statistics_t stats;         ///< System statistics
    
    // Timing
    uint32_t last_escalation_check;             ///< Last escalation check time
    uint32_t last_cleanup_time;                 ///< Last cleanup time
    
    critical_section_t cs;                      ///< Global critical section
};

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the alert system
 * 
 * @param config Pointer to configuration (NULL for defaults)
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_alerts_init(const pico_rtos_alert_config_t *config);

/**
 * @brief Enable or disable the alert system
 * 
 * @param enabled true to enable, false to disable
 */
void pico_rtos_alerts_set_enabled(bool enabled);

/**
 * @brief Check if the alert system is enabled
 * 
 * @return true if enabled, false otherwise
 */
bool pico_rtos_alerts_is_enabled(void);

// =============================================================================
// ALERT MANAGEMENT API
// =============================================================================

/**
 * @brief Create a new alert
 * 
 * @param severity Alert severity level
 * @param source Alert source
 * @param name Alert name
 * @param description Alert description
 * @param category Alert category (optional)
 * @return Alert ID, or 0 if creation failed
 */
uint32_t pico_rtos_alerts_create(pico_rtos_alert_severity_t severity,
                                pico_rtos_alert_source_t source,
                                const char *name,
                                const char *description,
                                const char *category);

/**
 * @brief Trigger an alert with threshold information
 * 
 * @param alert_id Alert ID
 * @param value Current value
 * @param threshold Threshold value
 * @param units Units string (optional)
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_trigger(uint32_t alert_id,
                             uint32_t value,
                             uint32_t threshold,
                             const char *units);

/**
 * @brief Acknowledge an alert
 * 
 * @param alert_id Alert ID
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_acknowledge(uint32_t alert_id);

/**
 * @brief Resolve an alert
 * 
 * @param alert_id Alert ID
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_resolve(uint32_t alert_id);

/**
 * @brief Delete an alert
 * 
 * @param alert_id Alert ID
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_delete(uint32_t alert_id);

/**
 * @brief Get alert information
 * 
 * @param alert_id Alert ID
 * @param alert Pointer to store alert information
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_get(uint32_t alert_id, pico_rtos_alert_t **alert);

// =============================================================================
// HANDLER MANAGEMENT API
// =============================================================================

/**
 * @brief Register an alert handler
 * 
 * @param name Handler name
 * @param callback Notification callback
 * @param filter Alert filter (optional)
 * @param user_data User data for callbacks
 * @param min_severity Minimum severity to handle
 * @param source_mask Source mask (0 for all sources)
 * @param priority Handler priority
 * @return Handler ID, or 0 if registration failed
 */
uint32_t pico_rtos_alerts_register_handler(const char *name,
                                          pico_rtos_alert_callback_t callback,
                                          pico_rtos_alert_filter_t filter,
                                          void *user_data,
                                          pico_rtos_alert_severity_t min_severity,
                                          pico_rtos_alert_source_t source_mask,
                                          uint32_t priority);

/**
 * @brief Unregister an alert handler
 * 
 * @param handler_id Handler ID
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_unregister_handler(uint32_t handler_id);

/**
 * @brief Enable or disable a handler
 * 
 * @param handler_id Handler ID
 * @param enabled true to enable, false to disable
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_set_handler_enabled(uint32_t handler_id, bool enabled);

// =============================================================================
// QUERY API
// =============================================================================

/**
 * @brief Get list of active alerts
 * 
 * @param alerts Array to store alert pointers
 * @param max_alerts Maximum number of alerts to return
 * @param actual_count Pointer to store actual number returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_get_active(pico_rtos_alert_t **alerts,
                                uint32_t max_alerts,
                                uint32_t *actual_count);

/**
 * @brief Get alerts by severity
 * 
 * @param severity Severity level to filter by
 * @param alerts Array to store alert pointers
 * @param max_alerts Maximum number of alerts to return
 * @param actual_count Pointer to store actual number returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_get_by_severity(pico_rtos_alert_severity_t severity,
                                     pico_rtos_alert_t **alerts,
                                     uint32_t max_alerts,
                                     uint32_t *actual_count);

/**
 * @brief Get alerts by source
 * 
 * @param source Source to filter by
 * @param alerts Array to store alert pointers
 * @param max_alerts Maximum number of alerts to return
 * @param actual_count Pointer to store actual number returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_get_by_source(pico_rtos_alert_source_t source,
                                   pico_rtos_alert_t **alerts,
                                   uint32_t max_alerts,
                                   uint32_t *actual_count);

/**
 * @brief Get alert history
 * 
 * @param alerts Array to store alert information
 * @param max_alerts Maximum number of alerts to return
 * @param actual_count Pointer to store actual number returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_get_history(pico_rtos_alert_t *alerts,
                                 uint32_t max_alerts,
                                 uint32_t *actual_count);

// =============================================================================
// CONFIGURATION API
// =============================================================================

/**
 * @brief Get current alert configuration
 * 
 * @param config Pointer to store configuration
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_get_config(pico_rtos_alert_config_t *config);

/**
 * @brief Set alert configuration
 * 
 * @param config Pointer to new configuration
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_set_config(const pico_rtos_alert_config_t *config);

// =============================================================================
// STATISTICS API
// =============================================================================

/**
 * @brief Get alert statistics
 * 
 * @param stats Pointer to store statistics
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_get_statistics(pico_rtos_alert_statistics_t *stats);

/**
 * @brief Reset alert statistics
 * 
 * @return true if successful, false otherwise
 */
bool pico_rtos_alerts_reset_statistics(void);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get string representation of alert severity
 * 
 * @param severity Alert severity
 * @return String representation
 */
const char *pico_rtos_alerts_get_severity_string(pico_rtos_alert_severity_t severity);

/**
 * @brief Get string representation of alert state
 * 
 * @param state Alert state
 * @return String representation
 */
const char *pico_rtos_alerts_get_state_string(pico_rtos_alert_state_t state);

/**
 * @brief Get string representation of alert source
 * 
 * @param source Alert source
 * @return String representation
 */
const char *pico_rtos_alerts_get_source_string(pico_rtos_alert_source_t source);

/**
 * @brief Print alert summary
 */
void pico_rtos_alerts_print_summary(void);

/**
 * @brief Print detailed alert report
 */
void pico_rtos_alerts_print_detailed_report(void);

// =============================================================================
// INTERNAL FUNCTIONS (for system use)
// =============================================================================

/**
 * @brief Periodic alert system maintenance
 * 
 * This function is called periodically by the system to handle
 * escalation, cleanup, and other maintenance tasks.
 */
void pico_rtos_alerts_periodic_update(void);

#endif // PICO_RTOS_ALERTS_H