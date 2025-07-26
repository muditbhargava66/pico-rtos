#ifndef PICO_RTOS_SMP_H
#define PICO_RTOS_SMP_H

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "types.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct pico_rtos_task;

// Forward declarations for SMP function signatures - types are defined in their respective headers
struct pico_rtos_mutex;
struct pico_rtos_semaphore;
struct pico_rtos_queue;

#ifdef PICO_RTOS_ENABLE_EVENT_GROUPS
// Forward declaration - actual definition is in event_group.h
struct pico_rtos_event_group;
#endif

// Core affinity definitions are in types.h

// =============================================================================
// CORE FAILURE DETECTION AND RECOVERY TYPES
// =============================================================================

/**
 * @brief Core health status
 */
typedef enum {
    PICO_RTOS_CORE_HEALTH_UNKNOWN = 0,    ///< Health status unknown
    PICO_RTOS_CORE_HEALTH_HEALTHY,        ///< Core is healthy and responsive
    PICO_RTOS_CORE_HEALTH_DEGRADED,       ///< Core is responding but with issues
    PICO_RTOS_CORE_HEALTH_UNRESPONSIVE,   ///< Core is not responding to health checks
    PICO_RTOS_CORE_HEALTH_FAILED,         ///< Core has failed and is non-functional
    PICO_RTOS_CORE_HEALTH_RECOVERING      ///< Core is in recovery process
} pico_rtos_core_health_status_t;

/**
 * @brief Core failure types
 */
typedef enum {
    PICO_RTOS_CORE_FAILURE_NONE = 0,      ///< No failure detected
    PICO_RTOS_CORE_FAILURE_WATCHDOG,      ///< Watchdog timeout failure
    PICO_RTOS_CORE_FAILURE_HANG,          ///< Core appears to be hung
    PICO_RTOS_CORE_FAILURE_EXCEPTION,     ///< Unhandled exception occurred
    PICO_RTOS_CORE_FAILURE_COMMUNICATION, ///< Inter-core communication failure
    PICO_RTOS_CORE_FAILURE_OVERLOAD       ///< Core overload condition
} pico_rtos_core_failure_type_t;

/**
 * @brief Core health monitoring configuration
 */
typedef struct {
    bool enabled;                          ///< Health monitoring enabled
    uint32_t watchdog_timeout_ms;          ///< Watchdog timeout in milliseconds
    uint32_t health_check_interval_ms;     ///< Health check interval
    uint32_t max_missed_heartbeats;        ///< Maximum missed heartbeats before failure
    uint32_t recovery_timeout_ms;          ///< Recovery timeout
    bool auto_recovery_enabled;            ///< Automatic recovery enabled
    bool graceful_degradation_enabled;     ///< Graceful degradation to single-core
} pico_rtos_core_health_config_t;

/**
 * @brief Core health monitoring state
 */
typedef struct {
    pico_rtos_core_health_status_t status; ///< Current health status
    pico_rtos_core_failure_type_t failure_type; ///< Type of failure detected
    uint32_t last_heartbeat_time;          ///< Last heartbeat timestamp
    uint32_t missed_heartbeats;            ///< Number of consecutive missed heartbeats
    uint32_t failure_count;                ///< Total number of failures detected
    uint32_t recovery_attempts;            ///< Number of recovery attempts
    uint32_t last_failure_time;            ///< Timestamp of last failure
    uint32_t total_downtime_ms;            ///< Total downtime in milliseconds
    bool watchdog_fed;                     ///< Watchdog has been fed this cycle
    uint32_t watchdog_feed_count;          ///< Number of times watchdog was fed
} pico_rtos_core_health_state_t;

/**
 * @brief Core failure recovery callback function type
 * 
 * @param failed_core_id ID of the failed core
 * @param failure_type Type of failure detected
 * @param user_data User-provided data
 * @return true if recovery should be attempted, false to disable core
 */
typedef bool (*pico_rtos_core_failure_callback_t)(uint32_t failed_core_id,
                                                 pico_rtos_core_failure_type_t failure_type,
                                                 void *user_data);

// =============================================================================
// PER-CORE STATE MANAGEMENT
// =============================================================================

/**
 * @brief Per-core scheduler state
 */
typedef struct {
    struct pico_rtos_task *running_task;   ///< Currently running task on this core
    struct pico_rtos_task *ready_list;     ///< Core-specific ready task list
    uint32_t core_id;                      ///< Core identifier (0 or 1)
    bool scheduler_active;                 ///< Scheduler active flag for this core
    uint32_t context_switches;             ///< Number of context switches on this core
    uint64_t idle_time_us;                 ///< Total idle time in microseconds
    uint64_t total_runtime_us;             ///< Total runtime since boot
    uint32_t task_count;                   ///< Number of tasks assigned to this core
    uint32_t load_percentage;              ///< Current load percentage (0-100)
    bool migration_in_progress;            ///< Task migration in progress flag
    pico_rtos_core_health_state_t health;  ///< Core health monitoring state
} pico_rtos_core_state_t;

/**
 * @brief SMP scheduler global state
 */
typedef struct {
    pico_rtos_core_state_t cores[2];       ///< Per-core state (RP2040 has 2 cores)
    pico_rtos_critical_section_t smp_cs;   ///< SMP coordination critical section
    uint32_t load_balance_threshold;       ///< Load balancing threshold (percentage)
    bool load_balancing_enabled;           ///< Load balancing enable flag
    uint32_t migration_count;              ///< Total number of task migrations
    uint32_t last_balance_time;            ///< Last load balancing timestamp
    bool smp_initialized;                  ///< SMP system initialization flag
    pico_rtos_core_health_config_t health_config; ///< Health monitoring configuration
    pico_rtos_core_failure_callback_t failure_callback; ///< Failure callback function
    void *failure_callback_data;           ///< User data for failure callback
    bool single_core_mode;                 ///< System degraded to single-core mode
    uint32_t healthy_core_id;              ///< ID of healthy core in single-core mode
} pico_rtos_smp_scheduler_t;

// =============================================================================
// TASK MIGRATION SUPPORT
// =============================================================================

/**
 * @brief Task migration request structure
 */
typedef struct {
    struct pico_rtos_task *task;           ///< Task to migrate
    uint32_t source_core;                  ///< Source core ID
    uint32_t target_core;                  ///< Target core ID
    uint32_t migration_reason;             ///< Reason for migration
    bool urgent;                           ///< Urgent migration flag
} pico_rtos_migration_request_t;

/**
 * @brief Migration reason codes
 */
typedef enum {
    PICO_RTOS_MIGRATION_LOAD_BALANCE = 0,  ///< Load balancing migration
    PICO_RTOS_MIGRATION_AFFINITY_CHANGE,   ///< Core affinity change
    PICO_RTOS_MIGRATION_CORE_FAILURE,      ///< Core failure recovery
    PICO_RTOS_MIGRATION_USER_REQUEST       ///< User-requested migration
} pico_rtos_migration_reason_t;

// =============================================================================
// SMP SCHEDULER FUNCTIONS
// =============================================================================

/**
 * @brief Initialize the SMP scheduler system
 * 
 * This function initializes the multi-core scheduler, sets up per-core state,
 * and prepares the system for SMP operation.
 * 
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_smp_init(void);

/**
 * @brief Start SMP scheduler on both cores
 * 
 * This function starts the scheduler on both cores and begins multi-core
 * task execution. Should be called after all initial tasks are created.
 */
void pico_rtos_smp_start(void);

/**
 * @brief Get the current core ID
 * 
 * @return Current core ID (0 or 1)
 */
uint32_t pico_rtos_smp_get_core_id(void);

/**
 * @brief Get per-core state information
 * 
 * @param core_id Core ID (0 or 1)
 * @return Pointer to core state structure, or NULL if invalid core ID
 */
pico_rtos_core_state_t *pico_rtos_smp_get_core_state(uint32_t core_id);

/**
 * @brief Get the SMP scheduler global state
 * 
 * @return Pointer to SMP scheduler state structure
 */
pico_rtos_smp_scheduler_t *pico_rtos_smp_get_scheduler_state(void);

// =============================================================================
// TASK AFFINITY MANAGEMENT
// =============================================================================

/**
 * @brief Set task core affinity
 * 
 * This function sets the core affinity for a task, determining which core(s)
 * the task can run on.
 * 
 * @param task Task to set affinity for, or NULL for current task
 * @param affinity Core affinity setting
 * @return true if affinity was set successfully, false otherwise
 */
bool pico_rtos_smp_set_task_affinity(struct pico_rtos_task *task, 
                                     pico_rtos_core_affinity_t affinity);

/**
 * @brief Get task core affinity
 * 
 * @param task Task to get affinity for, or NULL for current task
 * @return Current core affinity setting
 */
pico_rtos_core_affinity_t pico_rtos_smp_get_task_affinity(struct pico_rtos_task *task);

/**
 * @brief Check if task can run on specified core
 * 
 * @param task Task to check
 * @param core_id Core ID to check (0 or 1)
 * @return true if task can run on the specified core, false otherwise
 */
bool pico_rtos_smp_task_can_run_on_core(struct pico_rtos_task *task, uint32_t core_id);

// =============================================================================
// LOAD BALANCING FUNCTIONS
// =============================================================================

/**
 * @brief Enable or disable load balancing
 * 
 * @param enabled true to enable load balancing, false to disable
 */
void pico_rtos_smp_set_load_balancing(bool enabled);

/**
 * @brief Set load balancing threshold
 * 
 * @param threshold_percent Load difference threshold (0-100%)
 */
void pico_rtos_smp_set_load_balance_threshold(uint32_t threshold_percent);

/**
 * @brief Trigger manual load balancing
 * 
 * This function manually triggers the load balancing algorithm to redistribute
 * tasks between cores if needed.
 * 
 * @return Number of tasks migrated
 */
uint32_t pico_rtos_smp_balance_load(void);

/**
 * @brief Calculate core load percentage
 * 
 * @param core_id Core ID (0 or 1)
 * @return Load percentage (0-100), or 0 if invalid core ID
 */
uint32_t pico_rtos_smp_get_core_load(uint32_t core_id);

// =============================================================================
// TASK MIGRATION FUNCTIONS
// =============================================================================

/**
 * @brief Request task migration to specific core
 * 
 * @param task Task to migrate, or NULL for current task
 * @param target_core Target core ID (0 or 1)
 * @param reason Migration reason
 * @return true if migration was requested successfully, false otherwise
 */
bool pico_rtos_smp_migrate_task(struct pico_rtos_task *task, uint32_t target_core,
                               pico_rtos_migration_reason_t reason);

/**
 * @brief Get task migration statistics
 * 
 * @param task Task to get statistics for, or NULL for current task
 * @param migration_count Pointer to store migration count
 * @param last_migration_time Pointer to store last migration time
 * @return true if statistics were retrieved successfully, false otherwise
 */
bool pico_rtos_smp_get_migration_stats(struct pico_rtos_task *task,
                                      uint32_t *migration_count,
                                      uint32_t *last_migration_time);

// =============================================================================
// TASK DISTRIBUTION FUNCTIONS
// =============================================================================

/**
 * @brief Task assignment strategies
 */
typedef enum {
    PICO_RTOS_ASSIGN_ROUND_ROBIN,      ///< Round-robin assignment
    PICO_RTOS_ASSIGN_LEAST_LOADED,     ///< Assign to least loaded core
    PICO_RTOS_ASSIGN_PRIORITY_BASED,   ///< Priority-based assignment
    PICO_RTOS_ASSIGN_AFFINITY_FIRST    ///< Respect affinity first
} pico_rtos_task_assignment_strategy_t;

/**
 * @brief Set task assignment strategy
 * 
 * This function sets the strategy used for assigning new tasks to cores.
 * 
 * @param strategy Assignment strategy to use
 */
void pico_rtos_smp_set_assignment_strategy(pico_rtos_task_assignment_strategy_t strategy);

/**
 * @brief Get current task assignment strategy
 * 
 * @return Current assignment strategy
 */
pico_rtos_task_assignment_strategy_t pico_rtos_smp_get_assignment_strategy(void);

/**
 * @brief Get detailed load balancing statistics
 * 
 * @param total_migrations Pointer to store total number of migrations
 * @param last_balance_time Pointer to store last balance time
 * @param balance_cycles Pointer to store number of balance cycles
 * @return true if statistics were retrieved successfully, false otherwise
 */
bool pico_rtos_smp_get_load_balance_stats(uint32_t *total_migrations, 
                                         uint32_t *last_balance_time,
                                         uint32_t *balance_cycles);

// =============================================================================
// INTER-CORE SYNCHRONIZATION
// =============================================================================

/**
 * @brief Inter-core message types for FIFO communication
 */
typedef enum {
    PICO_RTOS_IPC_MSG_WAKEUP = 0x1000,        ///< Wake up scheduler
    PICO_RTOS_IPC_MSG_TASK_READY,             ///< Task became ready
    PICO_RTOS_IPC_MSG_MIGRATION_REQ,          ///< Task migration request
    PICO_RTOS_IPC_MSG_SYNC_BARRIER,           ///< Synchronization barrier
    PICO_RTOS_IPC_MSG_CRITICAL_ENTER,         ///< Critical section entry
    PICO_RTOS_IPC_MSG_CRITICAL_EXIT,          ///< Critical section exit
    PICO_RTOS_IPC_MSG_USER_DEFINED = 0x2000   ///< User-defined messages start here
} pico_rtos_ipc_message_type_t;

/**
 * @brief Inter-core message structure
 */
typedef struct {
    pico_rtos_ipc_message_type_t type;        ///< Message type
    uint32_t source_core;                     ///< Source core ID
    uint32_t data1;                           ///< First data word
    uint32_t data2;                           ///< Second data word
    uint32_t timestamp;                       ///< Message timestamp
} pico_rtos_ipc_message_t;

/**
 * @brief Inter-core synchronization barrier
 */
typedef struct {
    volatile uint32_t core_flags;             ///< Bitmask of cores that have reached barrier
    uint32_t required_cores;                  ///< Required cores to release barrier
    pico_rtos_critical_section_t cs;          ///< Critical section for barrier access
    uint32_t barrier_id;                      ///< Unique barrier identifier
    uint32_t wait_count;                      ///< Number of times barrier was used
} pico_rtos_sync_barrier_t;

/**
 * @brief Inter-core critical section with hardware support
 */
typedef struct {
    pico_rtos_critical_section_t hw_cs;       ///< Hardware critical section
    volatile uint32_t owner_core;             ///< Core that owns the critical section
    volatile uint32_t nesting_count;          ///< Nesting level for recursive entry
    uint32_t lock_count;                      ///< Total number of lock acquisitions
    uint32_t contention_count;                ///< Number of times lock was contended
} pico_rtos_inter_core_cs_t;

/**
 * @brief FIFO communication channel between cores
 */
typedef struct {
    volatile uint32_t read_pos;               ///< Read position in circular buffer
    volatile uint32_t write_pos;              ///< Write position in circular buffer
    pico_rtos_ipc_message_t *buffer;          ///< Message buffer
    uint32_t buffer_size;                     ///< Buffer size in messages
    pico_rtos_critical_section_t access_cs;   ///< Access synchronization
    uint32_t dropped_messages;                ///< Count of dropped messages due to full buffer
    uint32_t total_messages;                  ///< Total messages sent
} pico_rtos_ipc_channel_t;

// =============================================================================
// BASIC INTER-CORE SYNCHRONIZATION
// =============================================================================

/**
 * @brief Enter SMP critical section
 * 
 * This function enters a critical section that is safe across both cores.
 * It should be used when accessing shared data structures in SMP mode.
 */
void pico_rtos_smp_enter_critical(void);

/**
 * @brief Exit SMP critical section
 * 
 * This function exits the SMP critical section entered by
 * pico_rtos_smp_enter_critical().
 */
void pico_rtos_smp_exit_critical(void);

/**
 * @brief Send inter-core interrupt to wake up scheduler on other core
 * 
 * @param target_core Target core ID (0 or 1)
 */
void pico_rtos_smp_wake_core(uint32_t target_core);

// =============================================================================
// ADVANCED INTER-CORE CRITICAL SECTIONS
// =============================================================================

/**
 * @brief Initialize inter-core critical section
 * 
 * @param ics Pointer to inter-core critical section structure
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_inter_core_cs_init(pico_rtos_inter_core_cs_t *ics);

/**
 * @brief Enter inter-core critical section with hardware support
 * 
 * This function provides hardware-assisted critical sections that work
 * across both cores with proper nesting support.
 * 
 * @param ics Pointer to inter-core critical section structure
 * @param timeout_ms Timeout in milliseconds (0 for no timeout)
 * @return true if critical section was entered, false on timeout
 */
bool pico_rtos_inter_core_cs_enter(pico_rtos_inter_core_cs_t *ics, uint32_t timeout_ms);

/**
 * @brief Exit inter-core critical section
 * 
 * @param ics Pointer to inter-core critical section structure
 */
void pico_rtos_inter_core_cs_exit(pico_rtos_inter_core_cs_t *ics);

/**
 * @brief Try to enter inter-core critical section without blocking
 * 
 * @param ics Pointer to inter-core critical section structure
 * @return true if critical section was entered, false if already locked
 */
bool pico_rtos_inter_core_cs_try_enter(pico_rtos_inter_core_cs_t *ics);

/**
 * @brief Get inter-core critical section statistics
 * 
 * @param ics Pointer to inter-core critical section structure
 * @param lock_count Pointer to store total lock count
 * @param contention_count Pointer to store contention count
 * @return true if statistics were retrieved successfully
 */
bool pico_rtos_inter_core_cs_get_stats(pico_rtos_inter_core_cs_t *ics,
                                       uint32_t *lock_count,
                                       uint32_t *contention_count);

// =============================================================================
// INTER-CORE COMMUNICATION USING HARDWARE FIFOS
// =============================================================================

/**
 * @brief Initialize inter-core communication channel
 * 
 * @param channel Pointer to IPC channel structure
 * @param buffer Message buffer (must be accessible from both cores)
 * @param buffer_size Buffer size in number of messages
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_ipc_channel_init(pico_rtos_ipc_channel_t *channel,
                               pico_rtos_ipc_message_t *buffer,
                               uint32_t buffer_size);

/**
 * @brief Send message to other core using hardware FIFO
 * 
 * @param target_core Target core ID (0 or 1)
 * @param message Pointer to message to send
 * @param timeout_ms Timeout in milliseconds (0 for no timeout)
 * @return true if message was sent successfully, false on timeout or error
 */
bool pico_rtos_ipc_send_message(uint32_t target_core,
                               const pico_rtos_ipc_message_t *message,
                               uint32_t timeout_ms);

/**
 * @brief Receive message from other core using hardware FIFO
 * 
 * @param message Pointer to buffer to store received message
 * @param timeout_ms Timeout in milliseconds (0 for no timeout)
 * @return true if message was received successfully, false on timeout
 */
bool pico_rtos_ipc_receive_message(pico_rtos_ipc_message_t *message,
                                  uint32_t timeout_ms);

/**
 * @brief Check if there are pending messages from other core
 * 
 * @return Number of pending messages
 */
uint32_t pico_rtos_ipc_pending_messages(void);

/**
 * @brief Send simple notification to other core
 * 
 * This is a lightweight way to send simple notifications without
 * the overhead of full message structures.
 * 
 * @param target_core Target core ID (0 or 1)
 * @param notification_id Notification identifier
 * @return true if notification was sent successfully
 */
bool pico_rtos_ipc_send_notification(uint32_t target_core, uint32_t notification_id);

/**
 * @brief Process pending inter-core messages
 * 
 * This function should be called periodically to process incoming
 * inter-core messages and handle system-level IPC.
 */
void pico_rtos_ipc_process_messages(void);

/**
 * @brief Get IPC channel statistics
 * 
 * @param channel Pointer to IPC channel structure
 * @param total_messages Pointer to store total message count
 * @param dropped_messages Pointer to store dropped message count
 * @return true if statistics were retrieved successfully
 */
bool pico_rtos_ipc_get_channel_stats(pico_rtos_ipc_channel_t *channel,
                                    uint32_t *total_messages,
                                    uint32_t *dropped_messages);

// =============================================================================
// SYNCHRONIZATION BARRIERS
// =============================================================================

/**
 * @brief Initialize synchronization barrier
 * 
 * @param barrier Pointer to barrier structure
 * @param required_cores Bitmask of cores required to release barrier (bit 0 = core 0, bit 1 = core 1)
 * @param barrier_id Unique barrier identifier
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_sync_barrier_init(pico_rtos_sync_barrier_t *barrier,
                                uint32_t required_cores,
                                uint32_t barrier_id);

/**
 * @brief Wait at synchronization barrier
 * 
 * This function blocks the calling core until all required cores
 * have reached the barrier.
 * 
 * @param barrier Pointer to barrier structure
 * @param timeout_ms Timeout in milliseconds (0 for no timeout)
 * @return true if barrier was released, false on timeout
 */
bool pico_rtos_sync_barrier_wait(pico_rtos_sync_barrier_t *barrier,
                                uint32_t timeout_ms);

/**
 * @brief Reset synchronization barrier
 * 
 * @param barrier Pointer to barrier structure
 */
void pico_rtos_sync_barrier_reset(pico_rtos_sync_barrier_t *barrier);

/**
 * @brief Get barrier statistics
 * 
 * @param barrier Pointer to barrier structure
 * @param wait_count Pointer to store wait count
 * @param current_cores Pointer to store current cores at barrier
 * @return true if statistics were retrieved successfully
 */
bool pico_rtos_sync_barrier_get_stats(pico_rtos_sync_barrier_t *barrier,
                                     uint32_t *wait_count,
                                     uint32_t *current_cores);

// =============================================================================
// THREAD-SAFE SYNCHRONIZATION PRIMITIVES ACROSS CORES
// =============================================================================

/**
 * @brief Ensure mutex is thread-safe across cores
 * 
 * This function enhances existing mutexes to work correctly across
 * multiple cores by adding inter-core synchronization.
 * 
 * @param mutex Pointer to mutex structure
 * @return true if mutex was enhanced successfully
 */
bool pico_rtos_mutex_enable_smp(struct pico_rtos_mutex *mutex);

/**
 * @brief Ensure semaphore is thread-safe across cores
 * 
 * @param semaphore Pointer to semaphore structure
 * @return true if semaphore was enhanced successfully
 */
bool pico_rtos_semaphore_enable_smp(struct pico_rtos_semaphore *semaphore);

/**
 * @brief Ensure queue is thread-safe across cores
 * 
 * @param queue Pointer to queue structure
 * @return true if queue was enhanced successfully
 */
bool pico_rtos_queue_enable_smp(struct pico_rtos_queue *queue);

/**
 * @brief Ensure event group is thread-safe across cores
 * 
 * @param event_group Pointer to event group structure
 * @return true if event group was enhanced successfully
 */
#ifdef PICO_RTOS_ENABLE_EVENT_GROUPS
bool pico_rtos_event_group_enable_smp(struct pico_rtos_event_group *event_group);
#endif

/**
 * @brief Initialize core health monitoring system
 * 
 * This function initializes the core health monitoring system with the
 * specified configuration.
 * 
 * @param config Pointer to health monitoring configuration
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_core_health_init(const pico_rtos_core_health_config_t *config);

/**
 * @brief Start core health monitoring
 * 
 * This function starts the health monitoring system on both cores.
 * Must be called after pico_rtos_core_health_init().
 */
void pico_rtos_core_health_start(void);

/**
 * @brief Stop core health monitoring
 * 
 * This function stops the health monitoring system.
 */
void pico_rtos_core_health_stop(void);

/**
 * @brief Feed the watchdog for current core
 * 
 * This function should be called periodically by each core to indicate
 * that it is still alive and functioning.
 */
void pico_rtos_core_health_feed_watchdog(void);

/**
 * @brief Send heartbeat from current core
 * 
 * This function sends a heartbeat signal to indicate the core is alive.
 * Called automatically by the scheduler, but can be called manually.
 */
void pico_rtos_core_health_send_heartbeat(void);

/**
 * @brief Check health of specified core
 * 
 * @param core_id Core ID to check (0 or 1)
 * @return Current health status of the core
 */
pico_rtos_core_health_status_t pico_rtos_core_health_check(uint32_t core_id);

/**
 * @brief Get detailed health state for specified core
 * 
 * @param core_id Core ID to get state for (0 or 1)
 * @param state Pointer to structure to fill with health state
 * @return true if state was retrieved successfully, false otherwise
 */
bool pico_rtos_core_health_get_state(uint32_t core_id, pico_rtos_core_health_state_t *state);

/**
 * @brief Register core failure callback
 * 
 * This function registers a callback that will be called when a core
 * failure is detected.
 * 
 * @param callback Callback function to register
 * @param user_data User data to pass to callback
 * @return true if callback was registered successfully
 */
bool pico_rtos_core_health_register_failure_callback(pico_rtos_core_failure_callback_t callback,
                                                     void *user_data);

/**
 * @brief Trigger core failure recovery
 * 
 * This function manually triggers the recovery process for a failed core.
 * 
 * @param core_id Core ID to recover (0 or 1)
 * @return true if recovery was initiated successfully
 */
bool pico_rtos_core_health_recover_core(uint32_t core_id);

/**
 * @brief Enable graceful degradation to single-core operation
 * 
 * When a core fails and cannot be recovered, this function enables
 * graceful degradation where all tasks are migrated to the healthy core.
 * 
 * @param failed_core_id ID of the failed core
 * @return Number of tasks migrated to healthy core
 */
uint32_t pico_rtos_core_health_degrade_to_single_core(uint32_t failed_core_id);

/**
 * @brief Check if system is operating in single-core mode due to failure
 * 
 * @return true if system has degraded to single-core operation
 */
bool pico_rtos_core_health_is_single_core_mode(void);

/**
 * @brief Get core failure statistics
 * 
 * @param core_id Core ID to get statistics for (0 or 1)
 * @param failure_count Pointer to store total failure count
 * @param recovery_count Pointer to store successful recovery count
 * @param total_downtime_ms Pointer to store total downtime
 * @return true if statistics were retrieved successfully
 */
bool pico_rtos_core_health_get_failure_stats(uint32_t core_id,
                                             uint32_t *failure_count,
                                             uint32_t *recovery_count,
                                             uint32_t *total_downtime_ms);

/**
 * @brief Reset core health monitoring statistics
 * 
 * @param core_id Core ID to reset statistics for (0 or 1, or 0xFF for both)
 */
void pico_rtos_core_health_reset_stats(uint32_t core_id);

/**
 * @brief Process core health monitoring (internal function)
 * 
 * This function is called periodically by the system to check core health
 * and handle failures. Should not be called directly by user code.
 */
void pico_rtos_core_health_process(void);

// =============================================================================
// DEBUGGING AND STATISTICS
// =============================================================================

/**
 * @brief Get comprehensive SMP statistics
 * 
 * @param stats Pointer to structure to fill with statistics
 * @return true if statistics were retrieved successfully, false otherwise
 */
bool pico_rtos_smp_get_stats(pico_rtos_smp_scheduler_t *stats);

/**
 * @brief Print SMP scheduler state (debug function)
 * 
 * This function prints the current state of the SMP scheduler to the
 * debug output. Useful for debugging multi-core issues.
 */
void pico_rtos_smp_print_state(void);

/**
 * @brief Reset SMP statistics
 * 
 * This function resets all SMP-related statistics counters.
 */
void pico_rtos_smp_reset_stats(void);

// =============================================================================
// INTERNAL FUNCTIONS (DO NOT USE DIRECTLY)
// =============================================================================

/**
 * @brief Internal: Schedule next task on current core
 * 
 * This is an internal function used by the SMP scheduler. Do not call directly.
 */
void pico_rtos_smp_schedule_next_task(void);

/**
 * @brief Add new task to optimal core based on assignment strategy
 * 
 * This function assigns a new task to the optimal core based on the current
 * assignment strategy and adds it to that core's ready list.
 * 
 * @param task Task to add
 * @return true if task was added successfully, false otherwise
 */
bool pico_rtos_smp_add_new_task(struct pico_rtos_task *task);

/**
 * @brief Internal: Add task to core's ready list
 * 
 * This is an internal function used by the SMP scheduler. Do not call directly.
 * 
 * @param task Task to add
 * @param core_id Target core ID
 */
void pico_rtos_smp_add_task_to_core(struct pico_rtos_task *task, uint32_t core_id);

/**
 * @brief Internal: Remove task from core's ready list
 * 
 * This is an internal function used by the SMP scheduler. Do not call directly.
 * 
 * @param task Task to remove
 * @param core_id Source core ID
 */
void pico_rtos_smp_remove_task_from_core(struct pico_rtos_task *task, uint32_t core_id);

#ifdef __cplusplus
}
#endif

#endif // PICO_RTOS_SMP_H