#ifndef PICO_RTOS_BLOCKING_H
#define PICO_RTOS_BLOCKING_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"
#include "types.h"

// Forward declarations
typedef struct pico_rtos_task pico_rtos_task_t;

// Structure to hold information about a blocked task
typedef struct pico_rtos_blocked_task {
    pico_rtos_task_t *task;
    uint32_t timeout_time;
    uint32_t priority;                    // Cached priority for O(1) access
    struct pico_rtos_blocked_task *next;
    struct pico_rtos_blocked_task *prev;  // Doubly-linked for O(1) removal
} pico_rtos_blocked_task_t;

// Blocking object structure for synchronization primitives
typedef struct pico_rtos_block_object {
    void *sync_object;                          // Pointer to the synchronization object (mutex, semaphore, etc.)
    pico_rtos_blocked_task_t *blocked_tasks_head; // Head of blocked tasks list (highest priority first)
    pico_rtos_blocked_task_t *blocked_tasks_tail; // Tail of blocked tasks list (lowest priority last)
    uint32_t blocked_count;                     // Number of blocked tasks
    critical_section_t cs;                      // Critical section for thread safety
    bool priority_ordered;                      // Flag to enable priority-ordered insertion
} pico_rtos_block_object_t;

/**
 * @brief Create a new blocking object
 * 
 * @param sync_object Pointer to the synchronization object (mutex, semaphore, etc.)
 * @return pico_rtos_block_object_t* Pointer to the created blocking object, or NULL on failure
 */
pico_rtos_block_object_t *pico_rtos_block_object_create(void *sync_object);

/**
 * @brief Delete a blocking object and unblock all waiting tasks
 * 
 * @param block_obj Pointer to the blocking object to delete
 */
void pico_rtos_block_object_delete(pico_rtos_block_object_t *block_obj);

/**
 * @brief Block a task on a synchronization object
 * 
 * @param block_obj Pointer to the blocking object
 * @param task Pointer to the task to block
 * @param reason Reason for blocking
 * @param timeout Timeout in milliseconds (PICO_RTOS_WAIT_FOREVER for no timeout)
 * @return true if task was successfully blocked, false otherwise
 */
bool pico_rtos_block_task(pico_rtos_block_object_t *block_obj, pico_rtos_task_t *task, 
                         pico_rtos_block_reason_t reason, uint32_t timeout);

/**
 * @brief Unblock the highest priority task from a blocking object
 * 
 * @param block_obj Pointer to the blocking object
 * @return pico_rtos_task_t* Pointer to the unblocked task, or NULL if no tasks were blocked
 */
pico_rtos_task_t *pico_rtos_unblock_highest_priority_task(pico_rtos_block_object_t *block_obj);

/**
 * @brief Check for timed out tasks and unblock them
 * 
 * @param block_obj Pointer to the blocking object
 */
void pico_rtos_check_blocked_timeouts(pico_rtos_block_object_t *block_obj);

/**
 * @brief Get the number of tasks blocked on this object
 * 
 * @param block_obj Pointer to the blocking object
 * @return uint32_t Number of blocked tasks
 */
uint32_t pico_rtos_get_blocked_count(pico_rtos_block_object_t *block_obj);

/**
 * @brief Performance statistics for blocking system
 */
typedef struct {
    uint32_t total_blocked_tasks;        // Current number of blocked tasks
    uint32_t max_blocked_tasks;          // Maximum priority of blocked tasks
    uint32_t unblock_operations;         // Total unblock operations performed
    uint32_t timeout_operations;         // Total timeout operations performed
    bool priority_ordered;               // Whether priority ordering is enabled
    uint32_t avg_critical_section_cycles; // Average critical section duration
} pico_rtos_blocking_stats_t;

/**
 * @brief Get performance statistics for a blocking object
 * 
 * @param block_obj Pointer to the blocking object
 * @param stats Pointer to structure to fill with statistics
 */
void pico_rtos_get_blocking_stats(pico_rtos_block_object_t *block_obj, pico_rtos_blocking_stats_t *stats);

/**
 * @brief Validate that priority ordering is maintained (debug function)
 * 
 * @param block_obj Pointer to the blocking object
 * @return true if priority ordering is valid, false if corrupted
 */
bool pico_rtos_validate_priority_ordering(pico_rtos_block_object_t *block_obj);

#endif // PICO_RTOS_BLOCKING_H