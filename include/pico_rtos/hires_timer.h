#ifndef PICO_RTOS_HIRES_TIMER_H
#define PICO_RTOS_HIRES_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "pico_rtos/config.h"
#include "pico_rtos/error.h"
#include "pico/critical_section.h"

/**
 * @file hires_timer.h
 * @brief High-Resolution Software Timers for Pico-RTOS
 * 
 * This module provides microsecond-resolution software timers using the RP2040's
 * hardware timer capabilities. It includes drift compensation and precise timing
 * for applications requiring high-precision timing control.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef PICO_RTOS_HIRES_TIMER_MAX_TIMERS
#define PICO_RTOS_HIRES_TIMER_MAX_TIMERS 32
#endif

#ifndef PICO_RTOS_HIRES_TIMER_MIN_PERIOD_US
#define PICO_RTOS_HIRES_TIMER_MIN_PERIOD_US 10
#endif

#ifndef PICO_RTOS_HIRES_TIMER_MAX_PERIOD_US
#define PICO_RTOS_HIRES_TIMER_MAX_PERIOD_US 0xFFFFFFFFULL
#endif

#ifndef PICO_RTOS_HIRES_TIMER_DRIFT_COMPENSATION
#define PICO_RTOS_HIRES_TIMER_DRIFT_COMPENSATION 1
#endif

#ifndef PICO_RTOS_HIRES_TIMER_NAME_MAX_LENGTH
#define PICO_RTOS_HIRES_TIMER_NAME_MAX_LENGTH 32
#endif

// =============================================================================
// ERROR CODES
// =============================================================================

/**
 * @brief High-resolution timer specific error codes (800-819 range)
 */
typedef enum {
    PICO_RTOS_HIRES_TIMER_ERROR_NONE = 0,
    PICO_RTOS_HIRES_TIMER_ERROR_INVALID_PERIOD = 800,
    PICO_RTOS_HIRES_TIMER_ERROR_TIMER_NOT_FOUND = 801,
    PICO_RTOS_HIRES_TIMER_ERROR_TIMER_RUNNING = 802,
    PICO_RTOS_HIRES_TIMER_ERROR_TIMER_NOT_RUNNING = 803,
    PICO_RTOS_HIRES_TIMER_ERROR_MAX_TIMERS_EXCEEDED = 804,
    PICO_RTOS_HIRES_TIMER_ERROR_INVALID_CALLBACK = 805,
    PICO_RTOS_HIRES_TIMER_ERROR_HARDWARE_FAULT = 806,
    PICO_RTOS_HIRES_TIMER_ERROR_DRIFT_TOO_LARGE = 807,
    PICO_RTOS_HIRES_TIMER_ERROR_INVALID_PARAMETER = 808
} pico_rtos_hires_timer_error_t;

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

/**
 * @brief High-resolution timer callback function type
 * 
 * @param param User-defined parameter passed to callback
 */
typedef void (*pico_rtos_hires_timer_callback_t)(void *param);

/**
 * @brief Timer state enumeration
 */
typedef enum {
    PICO_RTOS_HIRES_TIMER_STATE_STOPPED = 0,
    PICO_RTOS_HIRES_TIMER_STATE_RUNNING,
    PICO_RTOS_HIRES_TIMER_STATE_EXPIRED,
    PICO_RTOS_HIRES_TIMER_STATE_ERROR
} pico_rtos_hires_timer_state_t;

/**
 * @brief Timer mode enumeration
 */
typedef enum {
    PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT = 0,
    PICO_RTOS_HIRES_TIMER_MODE_PERIODIC
} pico_rtos_hires_timer_mode_t;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct pico_rtos_hires_timer pico_rtos_hires_timer_t;

// =============================================================================
// TIMER STRUCTURE
// =============================================================================

/**
 * @brief High-resolution timer structure
 */
struct pico_rtos_hires_timer {
    const char *name;                           ///< Timer name (for debugging)
    uint32_t timer_id;                          ///< Unique timer identifier
    pico_rtos_hires_timer_callback_t callback; ///< Callback function
    void *param;                                ///< Callback parameter
    uint64_t period_us;                         ///< Period in microseconds
    uint64_t next_expiry_us;                    ///< Next expiry time (absolute)
    pico_rtos_hires_timer_mode_t mode;          ///< Timer mode (one-shot/periodic)
    pico_rtos_hires_timer_state_t state;        ///< Current timer state
    bool active;                                ///< Timer active flag
    
    // Drift compensation
    int64_t drift_compensation_us;              ///< Accumulated drift compensation
    uint64_t last_actual_expiry_us;             ///< Last actual expiry time
    uint64_t total_expirations;                 ///< Total number of expirations
    
    // Statistics
    uint64_t created_time_us;                   ///< Timer creation time
    uint64_t started_time_us;                   ///< Timer start time
    uint64_t total_runtime_us;                  ///< Total runtime
    uint32_t expiration_count;                  ///< Number of expirations
    uint32_t missed_deadlines;                  ///< Number of missed deadlines
    int64_t max_drift_us;                       ///< Maximum observed drift
    int64_t min_drift_us;                       ///< Minimum observed drift
    uint64_t max_callback_duration_us;          ///< Maximum callback execution time
    
    // Internal management
    critical_section_t cs;                      ///< Critical section for thread safety
    struct pico_rtos_hires_timer *next;         ///< Next timer in sorted list
    struct pico_rtos_hires_timer *prev;         ///< Previous timer in sorted list
};

// =============================================================================
// TIMER STATISTICS
// =============================================================================

/**
 * @brief Timer statistics structure
 */
typedef struct {
    uint64_t created_time_us;
    uint64_t started_time_us;
    uint64_t total_runtime_us;
    uint32_t expiration_count;
    uint32_t missed_deadlines;
    int64_t max_drift_us;
    int64_t min_drift_us;
    int64_t avg_drift_us;
    uint64_t max_callback_duration_us;
    uint64_t avg_callback_duration_us;
    double accuracy_percentage;
} pico_rtos_hires_timer_stats_t;

/**
 * @brief System timer statistics
 */
typedef struct {
    uint32_t active_timers;
    uint32_t total_timers_created;
    uint32_t total_expirations;
    uint32_t total_missed_deadlines;
    uint64_t system_uptime_us;
    uint64_t timer_overhead_us;
    uint32_t max_timers_active;
    double system_load_percentage;
} pico_rtos_hires_timer_system_stats_t;

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the high-resolution timer subsystem
 * 
 * Must be called before any other high-resolution timer functions.
 * 
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_hires_timer_init(void);

/**
 * @brief Create and initialize a high-resolution timer
 * 
 * @param timer Pointer to timer structure
 * @param name Timer name (for debugging, can be NULL)
 * @param callback Callback function to execute on timer expiry
 * @param param Parameter to pass to callback function
 * @param period_us Timer period in microseconds
 * @param mode Timer mode (one-shot or periodic)
 * @return true if timer created successfully, false otherwise
 */
bool pico_rtos_hires_timer_create(pico_rtos_hires_timer_t *timer,
                                 const char *name,
                                 pico_rtos_hires_timer_callback_t callback,
                                 void *param,
                                 uint64_t period_us,
                                 pico_rtos_hires_timer_mode_t mode);

/**
 * @brief Start a high-resolution timer
 * 
 * @param timer Pointer to timer structure
 * @return true if timer started successfully, false otherwise
 */
bool pico_rtos_hires_timer_start(pico_rtos_hires_timer_t *timer);

/**
 * @brief Stop a high-resolution timer
 * 
 * @param timer Pointer to timer structure
 * @return true if timer stopped successfully, false otherwise
 */
bool pico_rtos_hires_timer_stop(pico_rtos_hires_timer_t *timer);

/**
 * @brief Reset a high-resolution timer
 * 
 * Resets the timer to its initial state and restarts it if it was running.
 * 
 * @param timer Pointer to timer structure
 * @return true if timer reset successfully, false otherwise
 */
bool pico_rtos_hires_timer_reset(pico_rtos_hires_timer_t *timer);

/**
 * @brief Change the period of a high-resolution timer
 * 
 * @param timer Pointer to timer structure
 * @param period_us New period in microseconds
 * @return true if period changed successfully, false otherwise
 */
bool pico_rtos_hires_timer_change_period(pico_rtos_hires_timer_t *timer,
                                         uint64_t period_us);

/**
 * @brief Change the mode of a high-resolution timer
 * 
 * @param timer Pointer to timer structure
 * @param mode New timer mode
 * @return true if mode changed successfully, false otherwise
 */
bool pico_rtos_hires_timer_change_mode(pico_rtos_hires_timer_t *timer,
                                      pico_rtos_hires_timer_mode_t mode);

/**
 * @brief Delete a high-resolution timer
 * 
 * Stops the timer and removes it from the system.
 * 
 * @param timer Pointer to timer structure
 * @return true if timer deleted successfully, false otherwise
 */
bool pico_rtos_hires_timer_delete(pico_rtos_hires_timer_t *timer);

// =============================================================================
// QUERY FUNCTIONS
// =============================================================================

/**
 * @brief Check if a timer is running
 * 
 * @param timer Pointer to timer structure
 * @return true if timer is running, false otherwise
 */
bool pico_rtos_hires_timer_is_running(pico_rtos_hires_timer_t *timer);

/**
 * @brief Check if a timer is active (created and not deleted)
 * 
 * @param timer Pointer to timer structure
 * @return true if timer is active, false otherwise
 */
bool pico_rtos_hires_timer_is_active(pico_rtos_hires_timer_t *timer);

/**
 * @brief Get the current state of a timer
 * 
 * @param timer Pointer to timer structure
 * @return Current timer state
 */
pico_rtos_hires_timer_state_t pico_rtos_hires_timer_get_state(pico_rtos_hires_timer_t *timer);

/**
 * @brief Get remaining time until timer expiration
 * 
 * @param timer Pointer to timer structure
 * @return Remaining time in microseconds, 0 if timer is not running
 */
uint64_t pico_rtos_hires_timer_get_remaining_time_us(pico_rtos_hires_timer_t *timer);

/**
 * @brief Get the current period of a timer
 * 
 * @param timer Pointer to timer structure
 * @return Timer period in microseconds
 */
uint64_t pico_rtos_hires_timer_get_period_us(pico_rtos_hires_timer_t *timer);

/**
 * @brief Get the mode of a timer
 * 
 * @param timer Pointer to timer structure
 * @return Timer mode
 */
pico_rtos_hires_timer_mode_t pico_rtos_hires_timer_get_mode(pico_rtos_hires_timer_t *timer);

// =============================================================================
// TIME FUNCTIONS
// =============================================================================

/**
 * @brief Get current system time in microseconds
 * 
 * @return Current system time in microseconds since system start
 */
uint64_t pico_rtos_hires_timer_get_time_us(void);

/**
 * @brief Get system time with nanosecond precision (if available)
 * 
 * @return Current system time in nanoseconds since system start
 */
uint64_t pico_rtos_hires_timer_get_time_ns(void);

/**
 * @brief Delay execution for specified microseconds
 * 
 * This is a busy-wait delay with microsecond precision.
 * 
 * @param delay_us Delay time in microseconds
 */
void pico_rtos_hires_timer_delay_us(uint64_t delay_us);

/**
 * @brief Delay execution until specified absolute time
 * 
 * @param target_time_us Target time in microseconds (absolute)
 * @return true if delay completed successfully, false if target time already passed
 */
bool pico_rtos_hires_timer_delay_until_us(uint64_t target_time_us);

// =============================================================================
// STATISTICS AND MONITORING
// =============================================================================

/**
 * @brief Get statistics for a specific timer
 * 
 * @param timer Pointer to timer structure
 * @param stats Pointer to structure to store statistics
 * @return true if statistics retrieved successfully, false otherwise
 */
bool pico_rtos_hires_timer_get_stats(pico_rtos_hires_timer_t *timer,
                                    pico_rtos_hires_timer_stats_t *stats);

/**
 * @brief Get system-wide timer statistics
 * 
 * @param stats Pointer to structure to store system statistics
 * @return true if statistics retrieved successfully, false otherwise
 */
bool pico_rtos_hires_timer_get_system_stats(pico_rtos_hires_timer_system_stats_t *stats);

/**
 * @brief Reset statistics for a specific timer
 * 
 * @param timer Pointer to timer structure
 * @return true if statistics reset successfully, false otherwise
 */
bool pico_rtos_hires_timer_reset_stats(pico_rtos_hires_timer_t *timer);

/**
 * @brief Reset system-wide timer statistics
 * 
 * @return true if statistics reset successfully, false otherwise
 */
bool pico_rtos_hires_timer_reset_system_stats(void);

// =============================================================================
// ADVANCED FEATURES
// =============================================================================

/**
 * @brief Enable or disable drift compensation for a timer
 * 
 * @param timer Pointer to timer structure
 * @param enable true to enable drift compensation, false to disable
 * @return true if setting changed successfully, false otherwise
 */
bool pico_rtos_hires_timer_set_drift_compensation(pico_rtos_hires_timer_t *timer,
                                                 bool enable);

/**
 * @brief Get the current drift compensation setting
 * 
 * @param timer Pointer to timer structure
 * @return true if drift compensation is enabled, false otherwise
 */
bool pico_rtos_hires_timer_get_drift_compensation(pico_rtos_hires_timer_t *timer);

/**
 * @brief Set maximum allowed drift before error
 * 
 * @param timer Pointer to timer structure
 * @param max_drift_us Maximum allowed drift in microseconds
 * @return true if setting changed successfully, false otherwise
 */
bool pico_rtos_hires_timer_set_max_drift(pico_rtos_hires_timer_t *timer,
                                         uint64_t max_drift_us);

/**
 * @brief Calibrate timer accuracy
 * 
 * Performs a calibration routine to measure and compensate for
 * systematic timing errors.
 * 
 * @param calibration_duration_ms Duration of calibration in milliseconds
 * @return true if calibration completed successfully, false otherwise
 */
bool pico_rtos_hires_timer_calibrate(uint32_t calibration_duration_ms);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Find a timer by name
 * 
 * @param name Timer name to search for
 * @return Pointer to timer structure, or NULL if not found
 */
pico_rtos_hires_timer_t *pico_rtos_hires_timer_find_by_name(const char *name);

/**
 * @brief Get list of all active timers
 * 
 * @param timers Array to store timer pointers
 * @param max_timers Maximum number of timers to return
 * @param actual_count Pointer to store actual number of timers returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_hires_timer_get_timer_list(pico_rtos_hires_timer_t **timers,
                                          uint32_t max_timers,
                                          uint32_t *actual_count);

/**
 * @brief Get error description string
 * 
 * @param error High-resolution timer error code
 * @return Pointer to error description string
 */
const char *pico_rtos_hires_timer_get_error_string(pico_rtos_hires_timer_error_t error);

/**
 * @brief Get timer state string
 * 
 * @param state Timer state
 * @return Pointer to state description string
 */
const char *pico_rtos_hires_timer_get_state_string(pico_rtos_hires_timer_state_t state);

/**
 * @brief Get timer mode string
 * 
 * @param mode Timer mode
 * @return Pointer to mode description string
 */
const char *pico_rtos_hires_timer_get_mode_string(pico_rtos_hires_timer_mode_t mode);

// =============================================================================
// INTERNAL FUNCTIONS (for system use)
// =============================================================================

/**
 * @brief Process timer expirations (called by system tick handler)
 * 
 * This function is called by the system to process timer expirations.
 * It should not be called directly by user code.
 */
void pico_rtos_hires_timer_process_expirations(void);

/**
 * @brief Get next timer expiration time
 * 
 * Used by the system to determine when to schedule the next timer interrupt.
 * 
 * @return Next expiration time in microseconds, or 0 if no timers active
 */
uint64_t pico_rtos_hires_timer_get_next_expiration(void);

#endif // PICO_RTOS_HIRES_TIMER_H