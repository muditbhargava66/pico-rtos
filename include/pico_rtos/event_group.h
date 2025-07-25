#ifndef PICO_RTOS_EVENT_GROUP_H
#define PICO_RTOS_EVENT_GROUP_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"
#include "types.h"

/**
 * @file event_group.h
 * @brief Event Groups for Advanced Task Synchronization
 * 
 * Event groups provide a mechanism for tasks to wait on multiple events
 * simultaneously, supporting both "wait for any" and "wait for all" semantics.
 * This enables complex multi-task coordination patterns with O(1) performance
 * for event setting and clearing operations.
 * 
 * Features:
 * - 32-bit event storage (32 events per group)
 * - Wait for any or all specified events
 * - Priority-ordered task unblocking
 * - Configurable timeout support
 * - Optional event clearing on unblock
 * - Thread-safe operations
 */

// Forward declarations
struct pico_rtos_block_object;

// =============================================================================
// CONSTANTS AND LIMITS
// =============================================================================

/**
 * @brief Maximum number of events per event group
 * 
 * Each event group supports up to 32 individual events, represented
 * as bits in a 32-bit integer.
 */
#define PICO_RTOS_EVENT_GROUP_MAX_EVENTS 32

/**
 * @brief Event bit masks for common patterns
 */
#define PICO_RTOS_EVENT_BIT_0   (1UL << 0)   ///< Event bit 0
#define PICO_RTOS_EVENT_BIT_1   (1UL << 1)   ///< Event bit 1
#define PICO_RTOS_EVENT_BIT_2   (1UL << 2)   ///< Event bit 2
#define PICO_RTOS_EVENT_BIT_3   (1UL << 3)   ///< Event bit 3
#define PICO_RTOS_EVENT_BIT_4   (1UL << 4)   ///< Event bit 4
#define PICO_RTOS_EVENT_BIT_5   (1UL << 5)   ///< Event bit 5
#define PICO_RTOS_EVENT_BIT_6   (1UL << 6)   ///< Event bit 6
#define PICO_RTOS_EVENT_BIT_7   (1UL << 7)   ///< Event bit 7

/**
 * @brief All events mask
 */
#define PICO_RTOS_EVENT_ALL_BITS 0xFFFFFFFFUL

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Event group structure
 * 
 * Contains the current state of all events and manages blocked tasks
 * waiting for event conditions to be met.
 */
typedef struct {
    uint32_t event_bits;                    ///< Current event state (32 events max)
    struct pico_rtos_block_object *block_obj; ///< Blocking object for waiting tasks
    critical_section_t cs;                  ///< Thread safety protection
    uint32_t waiting_tasks;                 ///< Number of tasks currently waiting
    uint32_t total_sets;                    ///< Statistics: total set operations
    uint32_t total_clears;                  ///< Statistics: total clear operations
    uint32_t total_waits;                   ///< Statistics: total wait operations
} pico_rtos_event_group_t;

/**
 * @brief Event wait configuration structure
 * 
 * Defines the parameters for waiting on events, including which events
 * to wait for, wait semantics, and timeout behavior.
 */
typedef struct {
    uint32_t bits_to_wait_for;             ///< Events to wait for (bit mask)
    bool wait_for_all;                     ///< true = wait all, false = wait any
    bool clear_on_exit;                    ///< Clear events when unblocked
    uint32_t timeout;                      ///< Timeout value in milliseconds
} pico_rtos_event_wait_config_t;

// =============================================================================
// PUBLIC API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize an event group
 * 
 * Initializes an event group structure with all events cleared and
 * no waiting tasks. Must be called before using any other event group
 * functions.
 * 
 * @param event_group Pointer to event group structure to initialize
 * @return true if initialization was successful, false otherwise
 * 
 * @note This function is thread-safe and can be called from any context
 */
bool pico_rtos_event_group_init(pico_rtos_event_group_t *event_group);

/**
 * @brief Set event bits in an event group
 * 
 * Sets the specified event bits to 1. This operation is atomic and
 * will unblock any tasks waiting for these events if their wait
 * conditions are satisfied.
 * 
 * @param event_group Pointer to event group structure
 * @param bits Event bits to set (bit mask)
 * @return true if bits were set successfully, false on error
 * 
 * @note This function is thread-safe and can be called from ISR context
 * @note Performance: O(1) for bit setting, O(n) for task unblocking where
 *       n is the number of waiting tasks (bounded by max tasks)
 */
bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *event_group, uint32_t bits);

/**
 * @brief Clear event bits in an event group
 * 
 * Clears the specified event bits to 0. This operation is atomic and
 * does not affect waiting tasks (they continue to wait).
 * 
 * @param event_group Pointer to event group structure
 * @param bits Event bits to clear (bit mask)
 * @return true if bits were cleared successfully, false on error
 * 
 * @note This function is thread-safe and can be called from ISR context
 * @note Performance: O(1)
 */
bool pico_rtos_event_group_clear_bits(pico_rtos_event_group_t *event_group, uint32_t bits);

/**
 * @brief Wait for event bits with configurable semantics
 * 
 * Waits for the specified event bits to be set according to the wait
 * configuration. The calling task will be blocked until the wait condition
 * is satisfied or a timeout occurs.
 * 
 * @param event_group Pointer to event group structure
 * @param config Pointer to wait configuration structure
 * @return Current event bits when unblocked, or 0 on timeout/error
 * 
 * @note This function can only be called from task context (not ISR)
 * @note If clear_on_exit is true, the waited-for bits will be cleared
 *       atomically when the task is unblocked
 * @note Tasks are unblocked in priority order (highest priority first)
 */
uint32_t pico_rtos_event_group_wait_bits_config(pico_rtos_event_group_t *event_group, 
                                               const pico_rtos_event_wait_config_t *config);

/**
 * @brief Wait for event bits (simplified interface)
 * 
 * Simplified interface for waiting on event bits. Equivalent to calling
 * pico_rtos_event_group_wait_bits_config() with a configuration structure.
 * 
 * @param event_group Pointer to event group structure
 * @param bits_to_wait_for Event bits to wait for (bit mask)
 * @param wait_for_all true to wait for all bits, false to wait for any bit
 * @param clear_on_exit true to clear bits when unblocked, false to leave them set
 * @param timeout Timeout in milliseconds (PICO_RTOS_WAIT_FOREVER for no timeout)
 * @return Current event bits when unblocked, or 0 on timeout/error
 * 
 * @note This function can only be called from task context (not ISR)
 */
uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *event_group, 
                                        uint32_t bits_to_wait_for, 
                                        bool wait_for_all, 
                                        bool clear_on_exit, 
                                        uint32_t timeout);

/**
 * @brief Get current event bits without waiting
 * 
 * Returns the current state of all event bits without blocking.
 * This is useful for polling the event state.
 * 
 * @param event_group Pointer to event group structure
 * @return Current event bits, or 0 on error
 * 
 * @note This function is thread-safe and can be called from any context
 * @note Performance: O(1)
 */
uint32_t pico_rtos_event_group_get_bits(pico_rtos_event_group_t *event_group);

/**
 * @brief Get the number of tasks waiting on the event group
 * 
 * Returns the current number of tasks blocked waiting for events
 * on this event group.
 * 
 * @param event_group Pointer to event group structure
 * @return Number of waiting tasks, or 0 on error
 * 
 * @note This function is thread-safe and can be called from any context
 */
uint32_t pico_rtos_event_group_get_waiting_count(pico_rtos_event_group_t *event_group);

/**
 * @brief Delete an event group and unblock all waiting tasks
 * 
 * Deletes the event group and unblocks all tasks currently waiting
 * on it. The waiting tasks will return with a value of 0 indicating
 * that the event group was deleted.
 * 
 * @param event_group Pointer to event group structure to delete
 * 
 * @note After calling this function, the event group structure should
 *       not be used until re-initialized
 * @note All waiting tasks will be unblocked and will receive a return
 *       value of 0 from their wait functions
 */
void pico_rtos_event_group_delete(pico_rtos_event_group_t *event_group);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Create a bit mask for multiple event bits
 * 
 * Helper function to create a bit mask from an array of bit numbers.
 * 
 * @param bit_numbers Array of bit numbers (0-31)
 * @param count Number of bits in the array
 * @return Bit mask with specified bits set
 * 
 * @note Bit numbers outside the range 0-31 are ignored
 */
uint32_t pico_rtos_event_group_create_mask(const uint8_t *bit_numbers, uint32_t count);

/**
 * @brief Count the number of set bits in a mask
 * 
 * Helper function to count how many bits are set in an event mask.
 * 
 * @param mask Event bit mask
 * @return Number of set bits (0-32)
 */
uint32_t pico_rtos_event_group_count_bits(uint32_t mask);

// =============================================================================
// STATISTICS AND DEBUGGING
// =============================================================================

/**
 * @brief Event group statistics structure
 * 
 * Contains runtime statistics for debugging and performance analysis.
 */
typedef struct {
    uint32_t current_events;               ///< Current event bits set
    uint32_t waiting_tasks;                ///< Number of tasks currently waiting
    uint32_t total_set_operations;         ///< Total set operations performed
    uint32_t total_clear_operations;       ///< Total clear operations performed
    uint32_t total_wait_operations;        ///< Total wait operations performed
    uint32_t successful_waits;             ///< Number of successful waits (not timed out)
    uint32_t timed_out_waits;              ///< Number of waits that timed out
    uint32_t max_waiting_tasks;            ///< Maximum number of tasks that waited simultaneously
} pico_rtos_event_group_stats_t;

/**
 * @brief Get event group statistics
 * 
 * Retrieves runtime statistics for the specified event group.
 * 
 * @param event_group Pointer to event group structure
 * @param stats Pointer to structure to fill with statistics
 * @return true if statistics were retrieved successfully, false on error
 * 
 * @note This function is thread-safe and can be called from any context
 */
bool pico_rtos_event_group_get_stats(pico_rtos_event_group_t *event_group, 
                                    pico_rtos_event_group_stats_t *stats);

/**
 * @brief Reset event group statistics
 * 
 * Resets all statistics counters for the specified event group to zero.
 * Does not affect the current event state or waiting tasks.
 * 
 * @param event_group Pointer to event group structure
 * @return true if statistics were reset successfully, false on error
 */
bool pico_rtos_event_group_reset_stats(pico_rtos_event_group_t *event_group);

#endif // PICO_RTOS_EVENT_GROUP_H