#ifndef PICO_RTOS_WATCHDOG_H
#define PICO_RTOS_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include "pico_rtos/config.h"
#include "pico_rtos/types.h"
#include "pico/critical_section.h"

/**
 * @file watchdog.h
 * @brief Hardware Watchdog Integration for Pico-RTOS
 * 
 * This module provides comprehensive hardware watchdog timer integration
 * for the RP2040, including automatic feeding, timeout handling, and
 * system recovery capabilities.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef PICO_RTOS_WATCHDOG_DEFAULT_TIMEOUT_MS
#define PICO_RTOS_WATCHDOG_DEFAULT_TIMEOUT_MS 8000
#endif

#ifndef PICO_RTOS_WATCHDOG_FEED_INTERVAL_MS
#define PICO_RTOS_WATCHDOG_FEED_INTERVAL_MS 1000
#endif

#ifndef PICO_RTOS_WATCHDOG_MAX_HANDLERS
#define PICO_RTOS_WATCHDOG_MAX_HANDLERS 8
#endif

#ifndef PICO_RTOS_WATCHDOG_ENABLE_STATISTICS
#define PICO_RTOS_WATCHDOG_ENABLE_STATISTICS 1
#endif

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

/**
 * @brief Watchdog reset reasons
 */
typedef enum {
    PICO_RTOS_WATCHDOG_RESET_NONE = 0,
    PICO_RTOS_WATCHDOG_RESET_TIMEOUT,
    PICO_RTOS_WATCHDOG_RESET_SOFTWARE,
    PICO_RTOS_WATCHDOG_RESET_POWER_ON,
    PICO_RTOS_WATCHDOG_RESET_EXTERNAL,
    PICO_RTOS_WATCHDOG_RESET_UNKNOWN
} pico_rtos_watchdog_reset_reason_t;

/**
 * @brief Watchdog states
 */
typedef enum {
    PICO_RTOS_WATCHDOG_STATE_DISABLED = 0,
    PICO_RTOS_WATCHDOG_STATE_ENABLED,
    PICO_RTOS_WATCHDOG_STATE_FEEDING,
    PICO_RTOS_WATCHDOG_STATE_WARNING,
    PICO_RTOS_WATCHDOG_STATE_TIMEOUT
} pico_rtos_watchdog_state_t;

/**
 * @brief Watchdog recovery actions
 */
typedef enum {
    PICO_RTOS_WATCHDOG_ACTION_RESET = 0,
    PICO_RTOS_WATCHDOG_ACTION_CALLBACK,
    PICO_RTOS_WATCHDOG_ACTION_INTERRUPT,
    PICO_RTOS_WATCHDOG_ACTION_DEBUG_HALT
} pico_rtos_watchdog_action_t;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct pico_rtos_task pico_rtos_task_t;
typedef struct pico_rtos_watchdog_handler pico_rtos_watchdog_handler_t;

// =============================================================================
// CALLBACK TYPES
// =============================================================================

/**
 * @brief Watchdog timeout callback function type
 * 
 * Called when the watchdog is about to timeout or has timed out.
 * The callback should be fast and non-blocking.
 * 
 * @param remaining_ms Remaining time before timeout (0 if already timed out)
 * @param user_data User-defined data
 * @return Action to take (reset, continue, etc.)
 */
typedef pico_rtos_watchdog_action_t (*pico_rtos_watchdog_timeout_callback_t)(
    uint32_t remaining_ms, void *user_data);

/**
 * @brief Watchdog feed callback function type
 * 
 * Called before feeding the watchdog to allow custom checks.
 * 
 * @param user_data User-defined data
 * @return true to allow feeding, false to prevent feeding
 */
typedef bool (*pico_rtos_watchdog_feed_callback_t)(void *user_data);

/**
 * @brief System recovery callback function type
 * 
 * Called during system recovery after a watchdog reset.
 * 
 * @param reset_reason Reason for the reset
 * @param user_data User-defined data
 */
typedef void (*pico_rtos_watchdog_recovery_callback_t)(
    pico_rtos_watchdog_reset_reason_t reset_reason, void *user_data);

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Watchdog handler structure
 */
struct pico_rtos_watchdog_handler {
    const char *name;                               ///< Handler name
    pico_rtos_watchdog_timeout_callback_t timeout_callback; ///< Timeout callback
    pico_rtos_watchdog_feed_callback_t feed_callback; ///< Feed callback
    void *user_data;                                ///< User data for callbacks
    uint32_t priority;                              ///< Handler priority (higher = more important)
    bool enabled;                                   ///< Handler is enabled
    uint32_t timeout_count;                         ///< Number of timeouts handled
    uint32_t feed_count;                            ///< Number of feeds handled
};

/**
 * @brief Watchdog configuration structure
 */
typedef struct {
    uint32_t timeout_ms;                            ///< Watchdog timeout in milliseconds
    uint32_t feed_interval_ms;                      ///< Automatic feed interval
    bool auto_feed_enabled;                         ///< Enable automatic feeding
    bool pause_on_debug;                            ///< Pause watchdog when debugging
    pico_rtos_watchdog_action_t default_action;     ///< Default action on timeout
    uint32_t warning_threshold_ms;                  ///< Warning threshold before timeout
    bool enable_early_warning;                      ///< Enable early warning callbacks
} pico_rtos_watchdog_config_t;

/**
 * @brief Watchdog statistics structure
 */
typedef struct {
    uint32_t total_feeds;                           ///< Total number of feeds
    uint32_t total_timeouts;                        ///< Total number of timeouts
    uint32_t total_resets;                          ///< Total number of resets
    uint32_t missed_feeds;                          ///< Number of missed feeds
    uint32_t early_warnings;                        ///< Number of early warnings
    uint32_t max_feed_interval_ms;                  ///< Maximum observed feed interval
    uint32_t min_feed_interval_ms;                  ///< Minimum observed feed interval
    uint32_t avg_feed_interval_ms;                  ///< Average feed interval
    uint32_t uptime_since_last_reset_ms;            ///< Uptime since last reset
    pico_rtos_watchdog_reset_reason_t last_reset_reason; ///< Last reset reason
    uint32_t last_reset_timestamp;                  ///< Timestamp of last reset
} pico_rtos_watchdog_statistics_t;

/**
 * @brief Watchdog system state structure
 */
typedef struct {
    bool initialized;                               ///< System is initialized
    pico_rtos_watchdog_state_t state;               ///< Current watchdog state
    pico_rtos_watchdog_config_t config;             ///< Current configuration
    pico_rtos_watchdog_statistics_t stats;          ///< Statistics
    
    // Handler management
    pico_rtos_watchdog_handler_t handlers[PICO_RTOS_WATCHDOG_MAX_HANDLERS]; ///< Registered handlers
    uint32_t handler_count;                         ///< Number of registered handlers
    
    // Timing management
    uint32_t last_feed_time_ms;                     ///< Last feed timestamp
    uint32_t next_feed_time_ms;                     ///< Next scheduled feed time
    uint32_t timeout_time_ms;                       ///< Absolute timeout time
    bool warning_issued;                            ///< Warning has been issued
    
    // Hardware state
    bool hardware_enabled;                          ///< Hardware watchdog is enabled
    uint32_t hardware_timeout_ms;                   ///< Hardware timeout value
    
    critical_section_t cs;                          ///< Critical section for thread safety
} pico_rtos_watchdog_system_t;

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the watchdog system
 * 
 * @param config Pointer to configuration structure (NULL for defaults)
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_watchdog_init(const pico_rtos_watchdog_config_t *config);

/**
 * @brief Enable the hardware watchdog
 * 
 * @param timeout_ms Watchdog timeout in milliseconds
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_enable(uint32_t timeout_ms);

/**
 * @brief Disable the hardware watchdog
 * 
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_disable(void);

/**
 * @brief Check if the watchdog is enabled
 * 
 * @return true if enabled, false otherwise
 */
bool pico_rtos_watchdog_is_enabled(void);

/**
 * @brief Feed the watchdog (reset the timeout)
 * 
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_feed(void);

/**
 * @brief Force a watchdog reset
 * 
 * This function does not return.
 */
void pico_rtos_watchdog_force_reset(void) __attribute__((noreturn));

// =============================================================================
// CONFIGURATION API
// =============================================================================

/**
 * @brief Get current watchdog configuration
 * 
 * @param config Pointer to store configuration
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_get_config(pico_rtos_watchdog_config_t *config);

/**
 * @brief Set watchdog configuration
 * 
 * @param config Pointer to new configuration
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_set_config(const pico_rtos_watchdog_config_t *config);

/**
 * @brief Set watchdog timeout
 * 
 * @param timeout_ms New timeout in milliseconds
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_set_timeout(uint32_t timeout_ms);

/**
 * @brief Get current watchdog timeout
 * 
 * @return Current timeout in milliseconds, 0 if disabled
 */
uint32_t pico_rtos_watchdog_get_timeout(void);

/**
 * @brief Set automatic feed interval
 * 
 * @param interval_ms Feed interval in milliseconds (0 to disable auto-feed)
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_set_feed_interval(uint32_t interval_ms);

/**
 * @brief Enable or disable automatic feeding
 * 
 * @param enabled true to enable automatic feeding, false to disable
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_set_auto_feed(bool enabled);

// =============================================================================
// HANDLER MANAGEMENT API
// =============================================================================

/**
 * @brief Register a watchdog handler
 * 
 * @param name Handler name (for debugging)
 * @param timeout_callback Callback for timeout events (can be NULL)
 * @param feed_callback Callback before feeding (can be NULL)
 * @param user_data User data for callbacks
 * @param priority Handler priority (higher = more important)
 * @return Handler ID, or 0 if registration failed
 */
uint32_t pico_rtos_watchdog_register_handler(const char *name,
                                            pico_rtos_watchdog_timeout_callback_t timeout_callback,
                                            pico_rtos_watchdog_feed_callback_t feed_callback,
                                            void *user_data,
                                            uint32_t priority);

/**
 * @brief Unregister a watchdog handler
 * 
 * @param handler_id Handler ID returned from registration
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_unregister_handler(uint32_t handler_id);

/**
 * @brief Enable or disable a handler
 * 
 * @param handler_id Handler ID
 * @param enabled true to enable, false to disable
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_set_handler_enabled(uint32_t handler_id, bool enabled);

/**
 * @brief Get list of registered handlers
 * 
 * @param handlers Array to store handler pointers
 * @param max_handlers Maximum number of handlers to return
 * @param actual_count Pointer to store actual number of handlers returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_get_handlers(pico_rtos_watchdog_handler_t **handlers,
                                    uint32_t max_handlers,
                                    uint32_t *actual_count);

// =============================================================================
// STATUS AND MONITORING API
// =============================================================================

/**
 * @brief Get current watchdog state
 * 
 * @return Current watchdog state
 */
pico_rtos_watchdog_state_t pico_rtos_watchdog_get_state(void);

/**
 * @brief Get remaining time before timeout
 * 
 * @return Remaining time in milliseconds, 0 if disabled or timed out
 */
uint32_t pico_rtos_watchdog_get_remaining_time(void);

/**
 * @brief Get time since last feed
 * 
 * @return Time since last feed in milliseconds
 */
uint32_t pico_rtos_watchdog_get_time_since_feed(void);

/**
 * @brief Get watchdog statistics
 * 
 * @param stats Pointer to store statistics
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_get_statistics(pico_rtos_watchdog_statistics_t *stats);

/**
 * @brief Reset watchdog statistics
 * 
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_reset_statistics(void);

/**
 * @brief Check if system was reset by watchdog
 * 
 * @return Reset reason
 */
pico_rtos_watchdog_reset_reason_t pico_rtos_watchdog_get_reset_reason(void);

// =============================================================================
// RECOVERY API
// =============================================================================

/**
 * @brief Set system recovery callback
 * 
 * @param callback Recovery callback function
 * @param user_data User data for callback
 */
void pico_rtos_watchdog_set_recovery_callback(pico_rtos_watchdog_recovery_callback_t callback,
                                             void *user_data);

/**
 * @brief Perform system recovery after watchdog reset
 * 
 * This function should be called early in system initialization
 * to handle recovery from a watchdog reset.
 * 
 * @return true if recovery was needed and performed, false otherwise
 */
bool pico_rtos_watchdog_perform_recovery(void);

/**
 * @brief Save system state for recovery
 * 
 * Saves critical system state that can be restored after a watchdog reset.
 * 
 * @param data Pointer to data to save
 * @param size Size of data in bytes
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_save_recovery_data(const void *data, size_t size);

/**
 * @brief Restore system state after recovery
 * 
 * @param data Pointer to buffer to store recovered data
 * @param size Size of buffer in bytes
 * @param actual_size Pointer to store actual size of recovered data
 * @return true if successful, false otherwise
 */
bool pico_rtos_watchdog_restore_recovery_data(void *data, size_t size, size_t *actual_size);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get string representation of reset reason
 * 
 * @param reason Reset reason
 * @return String representation
 */
const char *pico_rtos_watchdog_get_reset_reason_string(pico_rtos_watchdog_reset_reason_t reason);

/**
 * @brief Get string representation of watchdog state
 * 
 * @param state Watchdog state
 * @return String representation
 */
const char *pico_rtos_watchdog_get_state_string(pico_rtos_watchdog_state_t state);

/**
 * @brief Get string representation of watchdog action
 * 
 * @param action Watchdog action
 * @return String representation
 */
const char *pico_rtos_watchdog_get_action_string(pico_rtos_watchdog_action_t action);

/**
 * @brief Print watchdog status summary
 */
void pico_rtos_watchdog_print_status(void);

/**
 * @brief Print detailed watchdog report
 */
void pico_rtos_watchdog_print_detailed_report(void);

// =============================================================================
// INTERNAL FUNCTIONS (for system use)
// =============================================================================

/**
 * @brief Periodic watchdog maintenance (called by system)
 * 
 * This function is called periodically by the system to handle
 * automatic feeding and timeout checking. It should not be called
 * directly by user code.
 */
void pico_rtos_watchdog_periodic_update(void);

/**
 * @brief Handle watchdog interrupt (called by interrupt handler)
 * 
 * This function is called by the hardware interrupt handler when
 * the watchdog generates an interrupt. It should not be called
 * directly by user code.
 */
void pico_rtos_watchdog_interrupt_handler(void);

#endif // PICO_RTOS_WATCHDOG_H