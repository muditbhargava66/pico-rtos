#ifndef PICO_RTOS_DEADLOCK_H
#define PICO_RTOS_DEADLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include "pico_rtos/config.h"
#include "pico_rtos/types.h"
#include "pico/critical_section.h"

/**
 * @file deadlock.h
 * @brief Deadlock Detection System for Pico-RTOS
 * 
 * This module provides comprehensive deadlock detection capabilities for
 * Pico-RTOS, tracking resource dependencies and detecting potential
 * deadlock situations before they occur.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef PICO_RTOS_DEADLOCK_MAX_RESOURCES
#define PICO_RTOS_DEADLOCK_MAX_RESOURCES 32
#endif

#ifndef PICO_RTOS_DEADLOCK_MAX_TASKS
#define PICO_RTOS_DEADLOCK_MAX_TASKS 16
#endif

#ifndef PICO_RTOS_DEADLOCK_ENABLE_PREVENTION
#define PICO_RTOS_DEADLOCK_ENABLE_PREVENTION 1
#endif

#ifndef PICO_RTOS_DEADLOCK_ENABLE_RECOVERY
#define PICO_RTOS_DEADLOCK_ENABLE_RECOVERY 1
#endif

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

/**
 * @brief Resource types for deadlock detection
 */
typedef enum {
    PICO_RTOS_RESOURCE_MUTEX = 0,
    PICO_RTOS_RESOURCE_SEMAPHORE,
    PICO_RTOS_RESOURCE_QUEUE,
    PICO_RTOS_RESOURCE_EVENT_GROUP,
    PICO_RTOS_RESOURCE_STREAM_BUFFER,
    PICO_RTOS_RESOURCE_MEMORY_POOL,
    PICO_RTOS_RESOURCE_CUSTOM
} pico_rtos_resource_type_t;

/**
 * @brief Deadlock detection states
 */
typedef enum {
    PICO_RTOS_DEADLOCK_STATE_NONE = 0,
    PICO_RTOS_DEADLOCK_STATE_POTENTIAL,
    PICO_RTOS_DEADLOCK_STATE_DETECTED,
    PICO_RTOS_DEADLOCK_STATE_RESOLVED
} pico_rtos_deadlock_state_t;

/**
 * @brief Deadlock recovery actions
 */
typedef enum {
    PICO_RTOS_DEADLOCK_ACTION_NONE = 0,
    PICO_RTOS_DEADLOCK_ACTION_ABORT_TASK,
    PICO_RTOS_DEADLOCK_ACTION_RELEASE_RESOURCE,
    PICO_RTOS_DEADLOCK_ACTION_TIMEOUT_OPERATION,
    PICO_RTOS_DEADLOCK_ACTION_CALLBACK
} pico_rtos_deadlock_action_t;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct pico_rtos_task pico_rtos_task_t;
typedef struct pico_rtos_deadlock_resource pico_rtos_deadlock_resource_t;
typedef struct pico_rtos_deadlock_detector pico_rtos_deadlock_detector_t;

// =============================================================================
// CALLBACK TYPES
// =============================================================================

/**
 * @brief Deadlock detection callback function type
 * 
 * Called when a deadlock is detected. The callback can decide what
 * action to take to resolve the deadlock.
 * 
 * @param detector Pointer to deadlock detector
 * @param resources Array of resources involved in deadlock
 * @param resource_count Number of resources in deadlock
 * @param tasks Array of tasks involved in deadlock
 * @param task_count Number of tasks in deadlock
 * @return Action to take to resolve deadlock
 */
typedef pico_rtos_deadlock_action_t (*pico_rtos_deadlock_callback_t)(
    pico_rtos_deadlock_detector_t *detector,
    pico_rtos_deadlock_resource_t **resources,
    uint32_t resource_count,
    pico_rtos_task_t **tasks,
    uint32_t task_count);

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Resource tracking structure
 */
struct pico_rtos_deadlock_resource {
    uint32_t resource_id;                       ///< Unique resource identifier
    pico_rtos_resource_type_t type;             ///< Resource type
    void *resource_ptr;                         ///< Pointer to actual resource
    const char *name;                           ///< Resource name (for debugging)
    
    pico_rtos_task_t *owner;                    ///< Current owner task
    pico_rtos_task_t *waiting_tasks[PICO_RTOS_DEADLOCK_MAX_TASKS]; ///< Tasks waiting for this resource
    uint32_t waiting_count;                     ///< Number of waiting tasks
    
    bool active;                                ///< Resource is active
    uint32_t acquisition_count;                 ///< Number of times acquired
    uint32_t contention_count;                  ///< Number of times contended
    uint32_t max_wait_time_ms;                  ///< Maximum wait time observed
    
    critical_section_t cs;                      ///< Critical section for thread safety
};

/**
 * @brief Task dependency tracking
 */
typedef struct {
    pico_rtos_task_t *task;                     ///< Task pointer
    pico_rtos_deadlock_resource_t *owned_resources[PICO_RTOS_DEADLOCK_MAX_RESOURCES]; ///< Resources owned by task
    pico_rtos_deadlock_resource_t *waiting_for; ///< Resource task is waiting for
    uint32_t owned_count;                       ///< Number of owned resources
    bool in_deadlock;                           ///< Task is part of a deadlock
} pico_rtos_task_dependency_t;

/**
 * @brief Deadlock detection result
 */
typedef struct {
    pico_rtos_deadlock_state_t state;           ///< Detection state
    uint32_t cycle_length;                      ///< Length of deadlock cycle
    pico_rtos_deadlock_resource_t *cycle_resources[PICO_RTOS_DEADLOCK_MAX_RESOURCES]; ///< Resources in cycle
    pico_rtos_task_t *cycle_tasks[PICO_RTOS_DEADLOCK_MAX_TASKS]; ///< Tasks in cycle
    uint32_t detection_time_us;                 ///< Time taken to detect deadlock
    const char *description;                    ///< Human-readable description
} pico_rtos_deadlock_result_t;

/**
 * @brief Deadlock detector main structure
 */
struct pico_rtos_deadlock_detector {
    bool initialized;                           ///< Detector is initialized
    bool enabled;                               ///< Detection is enabled
    
    pico_rtos_deadlock_resource_t resources[PICO_RTOS_DEADLOCK_MAX_RESOURCES]; ///< Resource pool
    pico_rtos_task_dependency_t task_deps[PICO_RTOS_DEADLOCK_MAX_TASKS]; ///< Task dependencies
    
    uint32_t resource_count;                    ///< Number of active resources
    uint32_t task_count;                        ///< Number of tracked tasks
    uint32_t next_resource_id;                  ///< Next resource ID to assign
    
    pico_rtos_deadlock_callback_t callback;     ///< Deadlock detection callback
    void *callback_data;                        ///< User data for callback
    
    // Detection algorithm state
    bool detection_in_progress;                 ///< Detection algorithm running
    uint32_t detection_depth;                   ///< Current detection depth
    uint32_t max_detection_depth;               ///< Maximum detection depth
    
    // Statistics
    uint32_t total_detections;                  ///< Total deadlocks detected
    uint32_t false_positives;                   ///< False positive detections
    uint32_t successful_recoveries;             ///< Successful recoveries
    uint32_t failed_recoveries;                 ///< Failed recoveries
    uint64_t total_detection_time_us;           ///< Total time spent detecting
    uint32_t max_detection_time_us;             ///< Maximum detection time
    
    critical_section_t cs;                      ///< Global critical section
};

/**
 * @brief Deadlock detector statistics
 */
typedef struct {
    uint32_t active_resources;                  ///< Currently active resources
    uint32_t tracked_tasks;                     ///< Currently tracked tasks
    uint32_t total_detections;                  ///< Total deadlocks detected
    uint32_t false_positives;                   ///< False positive detections
    uint32_t successful_recoveries;             ///< Successful recoveries
    uint32_t failed_recoveries;                 ///< Failed recoveries
    uint64_t total_detection_time_us;           ///< Total detection time
    uint64_t average_detection_time_us;         ///< Average detection time
    uint32_t max_detection_time_us;             ///< Maximum detection time
    uint32_t max_cycle_length;                  ///< Maximum cycle length detected
    double detection_accuracy;                  ///< Detection accuracy percentage
} pico_rtos_deadlock_statistics_t;

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the deadlock detection system
 * 
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_deadlock_init(void);

/**
 * @brief Enable or disable deadlock detection
 * 
 * @param enabled true to enable detection, false to disable
 */
void pico_rtos_deadlock_set_enabled(bool enabled);

/**
 * @brief Check if deadlock detection is enabled
 * 
 * @return true if enabled, false otherwise
 */
bool pico_rtos_deadlock_is_enabled(void);

/**
 * @brief Set deadlock detection callback
 * 
 * @param callback Callback function to call when deadlock detected
 * @param user_data User data to pass to callback
 */
void pico_rtos_deadlock_set_callback(pico_rtos_deadlock_callback_t callback, void *user_data);

// =============================================================================
// RESOURCE MANAGEMENT API
// =============================================================================

/**
 * @brief Register a resource for deadlock detection
 * 
 * @param resource_ptr Pointer to the actual resource
 * @param type Type of resource
 * @param name Resource name (for debugging)
 * @return Resource ID, or 0 if registration failed
 */
uint32_t pico_rtos_deadlock_register_resource(void *resource_ptr,
                                             pico_rtos_resource_type_t type,
                                             const char *name);

/**
 * @brief Unregister a resource from deadlock detection
 * 
 * @param resource_id Resource ID returned from registration
 * @return true if unregistration successful, false otherwise
 */
bool pico_rtos_deadlock_unregister_resource(uint32_t resource_id);

/**
 * @brief Notify that a task is requesting a resource
 * 
 * @param resource_id Resource ID
 * @param task Task requesting the resource
 * @return true if request is safe, false if potential deadlock
 */
bool pico_rtos_deadlock_request_resource(uint32_t resource_id, pico_rtos_task_t *task);

/**
 * @brief Notify that a task has acquired a resource
 * 
 * @param resource_id Resource ID
 * @param task Task that acquired the resource
 * @return true if acquisition recorded successfully, false otherwise
 */
bool pico_rtos_deadlock_acquire_resource(uint32_t resource_id, pico_rtos_task_t *task);

/**
 * @brief Notify that a task has released a resource
 * 
 * @param resource_id Resource ID
 * @param task Task that released the resource
 * @return true if release recorded successfully, false otherwise
 */
bool pico_rtos_deadlock_release_resource(uint32_t resource_id, pico_rtos_task_t *task);

/**
 * @brief Notify that a task is no longer waiting for a resource
 * 
 * @param resource_id Resource ID
 * @param task Task that stopped waiting
 * @return true if cancellation recorded successfully, false otherwise
 */
bool pico_rtos_deadlock_cancel_wait(uint32_t resource_id, pico_rtos_task_t *task);

// =============================================================================
// DETECTION API
// =============================================================================

/**
 * @brief Perform deadlock detection
 * 
 * @param result Pointer to structure to store detection result
 * @return true if detection completed, false if error occurred
 */
bool pico_rtos_deadlock_detect(pico_rtos_deadlock_result_t *result);

/**
 * @brief Check if a specific task is involved in a deadlock
 * 
 * @param task Task to check
 * @return true if task is in deadlock, false otherwise
 */
bool pico_rtos_deadlock_is_task_in_deadlock(pico_rtos_task_t *task);

/**
 * @brief Check if a specific resource is involved in a deadlock
 * 
 * @param resource_id Resource ID to check
 * @return true if resource is in deadlock, false otherwise
 */
bool pico_rtos_deadlock_is_resource_in_deadlock(uint32_t resource_id);

/**
 * @brief Get the current wait-for graph
 * 
 * @param resources Array to store resource pointers
 * @param tasks Array to store task pointers
 * @param max_entries Maximum number of entries to return
 * @param actual_count Pointer to store actual number of entries
 * @return true if successful, false otherwise
 */
bool pico_rtos_deadlock_get_wait_graph(pico_rtos_deadlock_resource_t **resources,
                                      pico_rtos_task_t **tasks,
                                      uint32_t max_entries,
                                      uint32_t *actual_count);

// =============================================================================
// RECOVERY API
// =============================================================================

/**
 * @brief Attempt to resolve a detected deadlock
 * 
 * @param result Deadlock detection result
 * @param action Action to take for resolution
 * @return true if resolution successful, false otherwise
 */
bool pico_rtos_deadlock_resolve(const pico_rtos_deadlock_result_t *result,
                               pico_rtos_deadlock_action_t action);

/**
 * @brief Force release of all resources owned by a task
 * 
 * @param task Task to release resources for
 * @return Number of resources released
 */
uint32_t pico_rtos_deadlock_force_release_task_resources(pico_rtos_task_t *task);

/**
 * @brief Abort a task involved in deadlock
 * 
 * @param task Task to abort
 * @return true if task aborted successfully, false otherwise
 */
bool pico_rtos_deadlock_abort_task(pico_rtos_task_t *task);

// =============================================================================
// MONITORING AND STATISTICS API
// =============================================================================

/**
 * @brief Get deadlock detection statistics
 * 
 * @param stats Pointer to structure to store statistics
 */
void pico_rtos_deadlock_get_statistics(pico_rtos_deadlock_statistics_t *stats);

/**
 * @brief Reset deadlock detection statistics
 */
void pico_rtos_deadlock_reset_statistics(void);

/**
 * @brief Get resource information
 * 
 * @param resource_id Resource ID
 * @param resource Pointer to store resource information
 * @return true if resource found, false otherwise
 */
bool pico_rtos_deadlock_get_resource_info(uint32_t resource_id,
                                         pico_rtos_deadlock_resource_t **resource);

/**
 * @brief Get list of all registered resources
 * 
 * @param resources Array to store resource pointers
 * @param max_resources Maximum number of resources to return
 * @param actual_count Pointer to store actual number of resources
 * @return true if successful, false otherwise
 */
bool pico_rtos_deadlock_get_resource_list(pico_rtos_deadlock_resource_t **resources,
                                         uint32_t max_resources,
                                         uint32_t *actual_count);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get string representation of resource type
 * 
 * @param type Resource type
 * @return String representation
 */
const char *pico_rtos_deadlock_get_resource_type_string(pico_rtos_resource_type_t type);

/**
 * @brief Get string representation of deadlock state
 * 
 * @param state Deadlock state
 * @return String representation
 */
const char *pico_rtos_deadlock_get_state_string(pico_rtos_deadlock_state_t state);

/**
 * @brief Get string representation of deadlock action
 * 
 * @param action Deadlock action
 * @return String representation
 */
const char *pico_rtos_deadlock_get_action_string(pico_rtos_deadlock_action_t action);

/**
 * @brief Print deadlock detection result
 * 
 * @param result Deadlock detection result to print
 */
void pico_rtos_deadlock_print_result(const pico_rtos_deadlock_result_t *result);

// =============================================================================
// INTERNAL FUNCTIONS (for system use)
// =============================================================================

/**
 * @brief Perform periodic deadlock detection (called by system)
 * 
 * This function is called periodically by the system to check for deadlocks.
 * It should not be called directly by user code.
 */
void pico_rtos_deadlock_periodic_check(void);

/**
 * @brief Clean up resources for terminated task
 * 
 * @param task Task that was terminated
 */
void pico_rtos_deadlock_cleanup_task(pico_rtos_task_t *task);

#endif // PICO_RTOS_DEADLOCK_H