#ifndef PICO_RTOS_TIMEOUT_H
#define PICO_RTOS_TIMEOUT_H

#include <stdint.h>
#include <stdbool.h>
#include "pico_rtos/types.h"
#include "pico_rtos/config.h"
#include "pico/critical_section.h"

/**
 * @file timeout.h
 * @brief Universal Timeout Support for Pico-RTOS
 * 
 * This module provides comprehensive timeout management for all blocking operations
 * in Pico-RTOS, ensuring consistent timeout behavior across all synchronization
 * primitives and system calls.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef PICO_RTOS_TIMEOUT_RESOLUTION_US
#define PICO_RTOS_TIMEOUT_RESOLUTION_US 1000  // 1ms resolution by default
#endif

#ifndef PICO_RTOS_TIMEOUT_MAX_ACTIVE
#define PICO_RTOS_TIMEOUT_MAX_ACTIVE 64
#endif

#ifndef PICO_RTOS_TIMEOUT_ENABLE_STATISTICS
#define PICO_RTOS_TIMEOUT_ENABLE_STATISTICS 1
#endif

// =============================================================================
// TIMEOUT CONSTANTS
// =============================================================================

/**
 * @brief Extended timeout constants
 */
#define PICO_RTOS_TIMEOUT_IMMEDIATE     0           ///< No wait, return immediately
#define PICO_RTOS_TIMEOUT_INFINITE      UINT32_MAX  ///< Wait forever
#define PICO_RTOS_TIMEOUT_DEFAULT       1000        ///< Default timeout (1 second)
#define PICO_RTOS_TIMEOUT_MAX_MS        (UINT32_MAX - 1) ///< Maximum timeout in milliseconds

// Convenience timeout values
#define PICO_RTOS_TIMEOUT_1MS           1
#define PICO_RTOS_TIMEOUT_10MS          10
#define PICO_RTOS_TIMEOUT_100MS         100
#define PICO_RTOS_TIMEOUT_1SEC          1000
#define PICO_RTOS_TIMEOUT_5SEC          5000
#define PICO_RTOS_TIMEOUT_10SEC         10000
#define PICO_RTOS_TIMEOUT_30SEC         30000
#define PICO_RTOS_TIMEOUT_1MIN          60000

// =============================================================================
// TIMEOUT RESULT CODES
// =============================================================================

/**
 * @brief Timeout operation results
 */
typedef enum {
    PICO_RTOS_TIMEOUT_SUCCESS = 0,      ///< Operation completed successfully
    PICO_RTOS_TIMEOUT_EXPIRED,          ///< Timeout expired before completion
    PICO_RTOS_TIMEOUT_CANCELLED,        ///< Timeout was cancelled
    PICO_RTOS_TIMEOUT_INVALID,          ///< Invalid timeout parameters
    PICO_RTOS_TIMEOUT_RESOURCE_ERROR    ///< Resource allocation error
} pico_rtos_timeout_result_t;

/**
 * @brief Timeout types for different operations
 */
typedef enum {
    PICO_RTOS_TIMEOUT_TYPE_BLOCKING = 0, ///< Standard blocking operation timeout
    PICO_RTOS_TIMEOUT_TYPE_PERIODIC,     ///< Periodic timeout (repeating)
    PICO_RTOS_TIMEOUT_TYPE_ABSOLUTE,     ///< Absolute time timeout
    PICO_RTOS_TIMEOUT_TYPE_RELATIVE,     ///< Relative time timeout
    PICO_RTOS_TIMEOUT_TYPE_DEADLINE      ///< Deadline-based timeout
} pico_rtos_timeout_type_t;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct pico_rtos_timeout pico_rtos_timeout_t;
typedef struct pico_rtos_task pico_rtos_task_t;

// =============================================================================
// TIMEOUT CALLBACK TYPES
// =============================================================================

/**
 * @brief Timeout callback function type
 * 
 * Called when a timeout expires. The callback should be fast and non-blocking.
 * 
 * @param timeout Pointer to the timeout structure
 * @param user_data User-defined data passed to the callback
 */
typedef void (*pico_rtos_timeout_callback_t)(pico_rtos_timeout_t *timeout, void *user_data);

/**
 * @brief Timeout cleanup function type
 * 
 * Called when a timeout is cancelled or completed to perform cleanup.
 * 
 * @param timeout Pointer to the timeout structure
 * @param user_data User-defined data passed to the cleanup function
 */
typedef void (*pico_rtos_timeout_cleanup_t)(pico_rtos_timeout_t *timeout, void *user_data);

// =============================================================================
// TIMEOUT STRUCTURE
// =============================================================================

/**
 * @brief Universal timeout structure
 */
struct pico_rtos_timeout {
    uint32_t timeout_id;                        ///< Unique timeout identifier
    pico_rtos_timeout_type_t type;              ///< Timeout type
    uint32_t timeout_ms;                        ///< Timeout value in milliseconds
    uint64_t start_time_us;                     ///< Start time in microseconds
    uint64_t expiry_time_us;                    ///< Expiry time in microseconds
    uint64_t deadline_us;                       ///< Deadline time (for deadline timeouts)
    
    pico_rtos_task_t *waiting_task;             ///< Task waiting on this timeout
    pico_rtos_timeout_callback_t callback;      ///< Timeout callback function
    pico_rtos_timeout_cleanup_t cleanup;        ///< Cleanup function
    void *user_data;                            ///< User-defined data
    
    pico_rtos_timeout_result_t result;          ///< Timeout result
    bool active;                                ///< Timeout is active
    bool cancelled;                             ///< Timeout was cancelled
    bool expired;                               ///< Timeout has expired
    
    // Statistics and monitoring
    uint32_t creation_time;                     ///< Creation timestamp
    uint32_t activation_count;                  ///< Number of times activated
    uint64_t total_wait_time_us;                ///< Total time spent waiting
    uint64_t max_wait_time_us;                  ///< Maximum wait time observed
    
    // Internal management
    critical_section_t cs;                      ///< Critical section for thread safety
    struct pico_rtos_timeout *next;             ///< Next timeout in list
    struct pico_rtos_timeout *prev;             ///< Previous timeout in list
};

// =============================================================================
// TIMEOUT STATISTICS
// =============================================================================

/**
 * @brief Timeout statistics structure
 */
typedef struct {
    uint32_t active_timeouts;                   ///< Currently active timeouts
    uint32_t total_timeouts_created;            ///< Total timeouts created
    uint32_t total_timeouts_expired;            ///< Total timeouts that expired
    uint32_t total_timeouts_cancelled;          ///< Total timeouts cancelled
    uint32_t total_timeouts_completed;          ///< Total timeouts completed successfully
    uint64_t total_wait_time_us;                ///< Total time spent in timeouts
    uint64_t average_wait_time_us;              ///< Average wait time
    uint64_t max_wait_time_us;                  ///< Maximum wait time observed
    uint32_t timeout_accuracy_errors;           ///< Number of accuracy errors
    uint32_t resource_allocation_failures;      ///< Resource allocation failures
    double timeout_accuracy_percentage;         ///< Overall timeout accuracy
} pico_rtos_timeout_statistics_t;

/**
 * @brief System timeout configuration
 */
typedef struct {
    uint32_t resolution_us;                     ///< Timeout resolution in microseconds
    uint32_t max_active_timeouts;               ///< Maximum active timeouts
    bool high_precision_mode;                   ///< Enable high precision timing
    bool statistics_enabled;                    ///< Enable statistics collection
    uint32_t cleanup_interval_ms;               ///< Cleanup interval for expired timeouts
    uint32_t accuracy_threshold_us;             ///< Accuracy threshold for error reporting
} pico_rtos_timeout_config_t;

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the universal timeout system
 * 
 * Must be called before using any timeout functions.
 * 
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_timeout_init(void);

/**
 * @brief Create a new timeout
 * 
 * @param timeout Pointer to timeout structure to initialize
 * @param type Timeout type
 * @param timeout_ms Timeout value in milliseconds
 * @param callback Callback function (can be NULL)
 * @param cleanup Cleanup function (can be NULL)
 * @param user_data User-defined data (can be NULL)
 * @return true if timeout created successfully, false otherwise
 */
bool pico_rtos_timeout_create(pico_rtos_timeout_t *timeout,
                             pico_rtos_timeout_type_t type,
                             uint32_t timeout_ms,
                             pico_rtos_timeout_callback_t callback,
                             pico_rtos_timeout_cleanup_t cleanup,
                             void *user_data);

/**
 * @brief Start a timeout
 * 
 * @param timeout Pointer to timeout structure
 * @return true if timeout started successfully, false otherwise
 */
bool pico_rtos_timeout_start(pico_rtos_timeout_t *timeout);

/**
 * @brief Cancel a timeout
 * 
 * @param timeout Pointer to timeout structure
 * @return true if timeout cancelled successfully, false otherwise
 */
bool pico_rtos_timeout_cancel(pico_rtos_timeout_t *timeout);

/**
 * @brief Reset a timeout to its initial state
 * 
 * @param timeout Pointer to timeout structure
 * @return true if timeout reset successfully, false otherwise
 */
bool pico_rtos_timeout_reset(pico_rtos_timeout_t *timeout);

/**
 * @brief Check if a timeout has expired
 * 
 * @param timeout Pointer to timeout structure
 * @return true if timeout has expired, false otherwise
 */
bool pico_rtos_timeout_is_expired(pico_rtos_timeout_t *timeout);

/**
 * @brief Check if a timeout is active
 * 
 * @param timeout Pointer to timeout structure
 * @return true if timeout is active, false otherwise
 */
bool pico_rtos_timeout_is_active(pico_rtos_timeout_t *timeout);

/**
 * @brief Get remaining time for a timeout
 * 
 * @param timeout Pointer to timeout structure
 * @return Remaining time in milliseconds, 0 if expired
 */
uint32_t pico_rtos_timeout_get_remaining_ms(pico_rtos_timeout_t *timeout);

/**
 * @brief Get remaining time for a timeout in microseconds
 * 
 * @param timeout Pointer to timeout structure
 * @return Remaining time in microseconds, 0 if expired
 */
uint64_t pico_rtos_timeout_get_remaining_us(pico_rtos_timeout_t *timeout);

/**
 * @brief Get elapsed time for a timeout
 * 
 * @param timeout Pointer to timeout structure
 * @return Elapsed time in milliseconds
 */
uint32_t pico_rtos_timeout_get_elapsed_ms(pico_rtos_timeout_t *timeout);

/**
 * @brief Get elapsed time for a timeout in microseconds
 * 
 * @param timeout Pointer to timeout structure
 * @return Elapsed time in microseconds
 */
uint64_t pico_rtos_timeout_get_elapsed_us(pico_rtos_timeout_t *timeout);

// =============================================================================
// BLOCKING OPERATION HELPERS
// =============================================================================

/**
 * @brief Wait for a condition with timeout
 * 
 * Generic helper for implementing timeouts in blocking operations.
 * 
 * @param condition_func Function that checks the wait condition
 * @param condition_data Data passed to condition function
 * @param timeout_ms Timeout in milliseconds
 * @return Timeout result
 */
pico_rtos_timeout_result_t pico_rtos_timeout_wait_for_condition(
    bool (*condition_func)(void *data),
    void *condition_data,
    uint32_t timeout_ms);

/**
 * @brief Wait until a specific time
 * 
 * @param target_time_us Target time in microseconds (absolute)
 * @return Timeout result
 */
pico_rtos_timeout_result_t pico_rtos_timeout_wait_until(uint64_t target_time_us);

/**
 * @brief Sleep for a specified duration
 * 
 * @param duration_ms Sleep duration in milliseconds
 * @return Timeout result
 */
pico_rtos_timeout_result_t pico_rtos_timeout_sleep(uint32_t duration_ms);

/**
 * @brief Sleep for a specified duration in microseconds
 * 
 * @param duration_us Sleep duration in microseconds
 * @return Timeout result
 */
pico_rtos_timeout_result_t pico_rtos_timeout_sleep_us(uint64_t duration_us);

// =============================================================================
// DEADLINE MANAGEMENT
// =============================================================================

/**
 * @brief Set a deadline for the current task
 * 
 * @param deadline_ms Deadline in milliseconds from now
 * @return true if deadline set successfully, false otherwise
 */
bool pico_rtos_timeout_set_deadline(uint32_t deadline_ms);

/**
 * @brief Clear the current task's deadline
 * 
 * @return true if deadline cleared successfully, false otherwise
 */
bool pico_rtos_timeout_clear_deadline(void);

/**
 * @brief Check if the current task has missed its deadline
 * 
 * @return true if deadline was missed, false otherwise
 */
bool pico_rtos_timeout_deadline_missed(void);

/**
 * @brief Get remaining time until deadline
 * 
 * @return Remaining time in milliseconds, 0 if deadline passed
 */
uint32_t pico_rtos_timeout_get_deadline_remaining_ms(void);

// =============================================================================
// CONFIGURATION AND MONITORING
// =============================================================================

/**
 * @brief Get timeout system configuration
 * 
 * @param config Pointer to configuration structure to fill
 */
void pico_rtos_timeout_get_config(pico_rtos_timeout_config_t *config);

/**
 * @brief Set timeout system configuration
 * 
 * @param config Pointer to new configuration
 * @return true if configuration applied successfully, false otherwise
 */
bool pico_rtos_timeout_set_config(const pico_rtos_timeout_config_t *config);

/**
 * @brief Get timeout statistics
 * 
 * @param stats Pointer to statistics structure to fill
 */
void pico_rtos_timeout_get_statistics(pico_rtos_timeout_statistics_t *stats);

/**
 * @brief Reset timeout statistics
 */
void pico_rtos_timeout_reset_statistics(void);

/**
 * @brief Get list of active timeouts
 * 
 * @param timeouts Array to store timeout pointers
 * @param max_timeouts Maximum number of timeouts to return
 * @param actual_count Pointer to store actual number of timeouts returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_timeout_get_active_list(pico_rtos_timeout_t **timeouts,
                                      uint32_t max_timeouts,
                                      uint32_t *actual_count);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Convert milliseconds to microseconds
 * 
 * @param ms Milliseconds
 * @return Microseconds
 */
static inline uint64_t pico_rtos_timeout_ms_to_us(uint32_t ms) {
    return (uint64_t)ms * 1000ULL;
}

/**
 * @brief Convert microseconds to milliseconds
 * 
 * @param us Microseconds
 * @return Milliseconds
 */
static inline uint32_t pico_rtos_timeout_us_to_ms(uint64_t us) {
    return (uint32_t)(us / 1000ULL);
}

/**
 * @brief Get current system time in microseconds
 * 
 * @return Current time in microseconds
 */
uint64_t pico_rtos_timeout_get_time_us(void);

/**
 * @brief Get current system time in milliseconds
 * 
 * @return Current time in milliseconds
 */
uint32_t pico_rtos_timeout_get_time_ms(void);

/**
 * @brief Get timeout result string
 * 
 * @param result Timeout result code
 * @return String description of the result
 */
const char *pico_rtos_timeout_get_result_string(pico_rtos_timeout_result_t result);

/**
 * @brief Get timeout type string
 * 
 * @param type Timeout type
 * @return String description of the type
 */
const char *pico_rtos_timeout_get_type_string(pico_rtos_timeout_type_t type);

// =============================================================================
// INTERNAL FUNCTIONS (for system use)
// =============================================================================

/**
 * @brief Process timeout expirations (called by system tick handler)
 * 
 * This function is called by the system to process timeout expirations.
 * It should not be called directly by user code.
 */
void pico_rtos_timeout_process_expirations(void);

/**
 * @brief Cleanup expired timeouts (called periodically by system)
 * 
 * This function performs cleanup of expired timeouts to free resources.
 * It should not be called directly by user code.
 */
void pico_rtos_timeout_cleanup_expired(void);

#endif // PICO_RTOS_TIMEOUT_H