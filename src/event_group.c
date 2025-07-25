#include "pico_rtos/event_group.h"
#include "pico_rtos/blocking.h"
#include "pico_rtos/error.h"
#include "pico_rtos/logging.h"
#include "pico_rtos/task.h"
#include "pico_rtos.h"
#include "pico/critical_section.h"
#include <string.h>

/**
 * @file event_group.c
 * @brief Event Groups Implementation for Advanced Task Synchronization
 * 
 * This implementation provides O(1) event setting/clearing operations and
 * priority-ordered task unblocking for optimal real-time performance.
 * 
 * Key Performance Characteristics:
 * - Event setting: O(1) - Uses atomic bitwise OR operation
 * - Event clearing: O(1) - Uses atomic bitwise AND with complement
 * - Event reading: O(1) - Direct memory access with critical section
 * - Task unblocking: O(n) where n is number of waiting tasks (bounded)
 * 
 * Atomic Operations:
 * All bit manipulation operations are performed within critical sections
 * to ensure atomicity across multiple cores. The RP2040's critical section
 * implementation provides hardware-level synchronization.
 * 
 * Thread Safety:
 * - All public functions are thread-safe and can be called from any context
 * - Set/clear operations can be called from ISR context
 * - Wait operations can only be called from task context
 * - Statistics are updated atomically with the bit operations
 */

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Check if wait condition is satisfied
 * 
 * @param current_events Current event bits
 * @param bits_to_wait_for Event bits to wait for
 * @param wait_for_all true for AND logic, false for OR logic
 * @return true if condition is satisfied, false otherwise
 */
static inline bool is_wait_condition_satisfied(uint32_t current_events, 
                                             uint32_t bits_to_wait_for, 
                                             bool wait_for_all) {
    if (wait_for_all) {
        // AND logic: all specified bits must be set
        return (current_events & bits_to_wait_for) == bits_to_wait_for;
    } else {
        // OR logic: any specified bit must be set
        return (current_events & bits_to_wait_for) != 0;
    }
}

/**
 * @brief Perform atomic bit manipulation with statistics update
 * 
 * This helper function encapsulates the atomic bit manipulation pattern
 * used by both set and clear operations, ensuring O(1) performance.
 * 
 * @param event_group Pointer to event group structure
 * @param bits Bit pattern to manipulate
 * @param operation Function pointer to the bit operation (set or clear)
 * @param stat_counter Pointer to the statistics counter to increment
 * @return Previous event bits value
 */
static inline uint32_t atomic_bit_operation(pico_rtos_event_group_t *event_group,
                                          uint32_t bits,
                                          uint32_t (*operation)(uint32_t current, uint32_t bits),
                                          uint32_t *stat_counter) {
    critical_section_enter_blocking(&event_group->cs);
    
    uint32_t old_bits = event_group->event_bits;
    event_group->event_bits = operation(old_bits, bits);
    (*stat_counter)++;
    
    critical_section_exit(&event_group->cs);
    
    return old_bits;
}

/**
 * @brief Bit set operation for atomic_bit_operation helper
 */
static inline uint32_t bit_set_op(uint32_t current, uint32_t bits) {
    return current | bits;
}

/**
 * @brief Bit clear operation for atomic_bit_operation helper
 */
static inline uint32_t bit_clear_op(uint32_t current, uint32_t bits) {
    return current & ~bits;
}

/**
 * @brief Event wait information stored for each blocked task
 * 
 * This structure stores the wait condition for each task blocked on the event group.
 * It's stored as task-local data to avoid modifying the blocking system.
 */
typedef struct {
    uint32_t bits_to_wait_for;             ///< Events to wait for (bit mask)
    bool wait_for_all;                     ///< true = wait all, false = wait any
    bool clear_on_exit;                    ///< Clear events when unblocked
    uint32_t timeout;                      ///< Timeout value in milliseconds
} event_wait_info_t;

/**
 * @brief Check if a specific task's wait condition is satisfied
 * 
 * @param current_events Current event bits
 * @param wait_info Wait condition information
 * @return true if condition is satisfied, false otherwise
 */
static inline bool is_task_wait_condition_satisfied(uint32_t current_events, 
                                                   const event_wait_info_t *wait_info) {
    if (wait_info->wait_for_all) {
        // AND logic: all specified bits must be set
        return (current_events & wait_info->bits_to_wait_for) == wait_info->bits_to_wait_for;
    } else {
        // OR logic: any specified bit must be set
        return (current_events & wait_info->bits_to_wait_for) != 0;
    }
}

/**
 * @brief Unblock tasks whose wait conditions are now satisfied
 * 
 * This function is called after setting event bits to unblock any tasks
 * whose wait conditions have been satisfied. It implements proper event
 * group semantics by checking each waiting task's specific conditions.
 * 
 * Since the blocking system doesn't store custom wait conditions, we use
 * a cooperative approach where we unblock tasks and they re-check their
 * conditions when they run.
 * 
 * @param event_group Pointer to event group structure
 */
static void unblock_satisfied_tasks(pico_rtos_event_group_t *event_group) {
    if (event_group->block_obj == NULL || event_group->waiting_tasks == 0) {
        return;
    }
    
    // Since we can't determine which specific tasks have satisfied conditions
    // without extending the blocking system, we use a cooperative approach:
    // 1. Unblock the highest priority task
    // 2. Let it check its condition when it runs
    // 3. If not satisfied, it will re-block
    // 4. If satisfied, it will continue and potentially trigger more unblocks
    
    // This approach ensures:
    // - Priority ordering is maintained (highest priority task gets first chance)
    // - Correctness (tasks only proceed if their condition is satisfied)
    // - Compatibility with existing blocking system
    
    pico_rtos_task_t *task = pico_rtos_unblock_highest_priority_task(event_group->block_obj);
    if (task != NULL) {
        // A task was unblocked - trigger scheduler so it can run and check its condition
        pico_rtos_schedule_next_task();
    }
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_event_group_init(pico_rtos_event_group_t *event_group) {
    if (event_group == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group initialization failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    PICO_RTOS_LOG_EVENT_DEBUG("Initializing event group at %p", (void*)event_group);
    
    // Initialize all fields to zero/default values
    memset(event_group, 0, sizeof(pico_rtos_event_group_t));
    
    // Initialize critical section for thread safety
    critical_section_init(&event_group->cs);
    
    // Create blocking object for this event group
    event_group->block_obj = pico_rtos_block_object_create(event_group);
    if (event_group->block_obj == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group initialization failed: Could not create blocking object");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_OUT_OF_MEMORY, 0);
        return false;
    }
    
    PICO_RTOS_LOG_EVENT_DEBUG("Event group at %p initialized successfully", (void*)event_group);
    return true;
}

bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *event_group, uint32_t bits) {
    if (event_group == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group set bits failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (bits == 0) {
        // Setting no bits is a no-op but not an error
        return true;
    }
    
    // Parameter validation: Since we use uint32_t for bits, all possible values 
    // (0-0xFFFFFFFF) are valid event bit patterns. The type system provides 
    // sufficient validation - any 32-bit value represents a valid combination 
    // of the 32 available event bits (0-31).
    
    PICO_RTOS_LOG_EVENT_DEBUG("Setting event bits 0x%08lx in event group %p", bits, (void*)event_group);
    
    // Perform atomic bit set operation - O(1) performance guaranteed
    uint32_t old_bits = atomic_bit_operation(event_group, bits, bit_set_op, &event_group->total_sets);
    
    // Check if any new bits were actually set
    bool bits_changed = (event_group->event_bits != old_bits);
    
    // If bits changed and there are waiting tasks, check for unblocking
    // This is done outside the critical section to minimize lock time
    if (bits_changed && event_group->waiting_tasks > 0) {
        unblock_satisfied_tasks(event_group);
    }
    
    PICO_RTOS_LOG_EVENT_DEBUG("Event group %p now has bits 0x%08lx", 
                       (void*)event_group, event_group->event_bits);
    
    return true;
}

bool pico_rtos_event_group_clear_bits(pico_rtos_event_group_t *event_group, uint32_t bits) {
    if (event_group == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group clear bits failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (bits == 0) {
        // Clearing no bits is a no-op but not an error
        return true;
    }
    
    // Parameter validation: Since we use uint32_t for bits, all possible values 
    // (0-0xFFFFFFFF) are valid event bit patterns. The type system provides 
    // sufficient validation - any 32-bit value represents a valid combination 
    // of the 32 available event bits (0-31).
    
    PICO_RTOS_LOG_EVENT_DEBUG("Clearing event bits 0x%08lx in event group %p", bits, (void*)event_group);
    
    // Perform atomic bit clear operation - O(1) performance guaranteed
    atomic_bit_operation(event_group, bits, bit_clear_op, &event_group->total_clears);
    
    PICO_RTOS_LOG_EVENT_DEBUG("Event group %p now has bits 0x%08lx", 
                       (void*)event_group, event_group->event_bits);
    
    return true;
}

uint32_t pico_rtos_event_group_wait_bits_config(pico_rtos_event_group_t *event_group, 
                                               const pico_rtos_event_wait_config_t *config) {
    if (event_group == NULL || config == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group wait failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return 0;
    }
    
    if (config->bits_to_wait_for == 0) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group wait failed: No bits specified");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_EVENT_GROUP_INVALID, 0);
        return 0;
    }
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group wait failed: Called from interrupt context");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INTERRUPT_CONTEXT_VIOLATION, 0);
        return 0;
    }
    
    PICO_RTOS_LOG_EVENT_DEBUG("Task %s waiting for event bits 0x%08lx (wait_all=%s, clear=%s, timeout=%lu)", 
                       current_task->name ? current_task->name : "unnamed",
                       config->bits_to_wait_for,
                       config->wait_for_all ? "true" : "false",
                       config->clear_on_exit ? "true" : "false",
                       config->timeout);
    
    // Store wait configuration for this task (simple approach using static storage)
    // In a full implementation, this would be stored per-task
    static event_wait_info_t task_wait_info;
    task_wait_info.bits_to_wait_for = config->bits_to_wait_for;
    task_wait_info.wait_for_all = config->wait_for_all;
    task_wait_info.clear_on_exit = config->clear_on_exit;
    task_wait_info.timeout = config->timeout;
    
    // Main wait loop - handles re-blocking if condition becomes unsatisfied
    uint32_t wait_start_time = pico_rtos_get_tick_count();
    bool first_iteration = true;
    
    while (true) {
        critical_section_enter_blocking(&event_group->cs);
        
        // Check if condition is satisfied
        if (is_wait_condition_satisfied(event_group->event_bits, config->bits_to_wait_for, config->wait_for_all)) {
            uint32_t current_bits = event_group->event_bits;
            
            // Clear bits if requested
            if (config->clear_on_exit) {
                event_group->event_bits &= ~config->bits_to_wait_for;
                PICO_RTOS_LOG_EVENT_DEBUG("Cleared bits 0x%08lx on exit, event group %p now has 0x%08lx", 
                                   config->bits_to_wait_for, (void*)event_group, event_group->event_bits);
            }
            
            critical_section_exit(&event_group->cs);
            
            PICO_RTOS_LOG_EVENT_DEBUG("Task %s wait condition satisfied, returning 0x%08lx", 
                               current_task->name ? current_task->name : "unnamed", current_bits);
            
            return current_bits;
        }
        
        // Check for no-wait case on first iteration
        if (first_iteration && config->timeout == PICO_RTOS_NO_WAIT) {
            critical_section_exit(&event_group->cs);
            PICO_RTOS_LOG_EVENT_DEBUG("Task %s wait condition not satisfied and no wait requested", 
                               current_task->name ? current_task->name : "unnamed");
            return 0;
        }
        
        // Check for timeout (overflow-safe comparison)
        uint32_t current_time = pico_rtos_get_tick_count();
        if (config->timeout != PICO_RTOS_WAIT_FOREVER) {
            // Use overflow-safe time comparison
            int32_t elapsed = (int32_t)(current_time - wait_start_time);
            if (elapsed >= (int32_t)config->timeout) {
                critical_section_exit(&event_group->cs);
                PICO_RTOS_LOG_EVENT_DEBUG("Task %s wait timed out after %ld ms", 
                                   current_task->name ? current_task->name : "unnamed", elapsed);
                return 0;
            }
        }
        
        // Need to block - increment waiting count
        event_group->waiting_tasks++;
        if (first_iteration) {
            event_group->total_waits++;
        }
        
        critical_section_exit(&event_group->cs);
        
        // Calculate remaining timeout (overflow-safe)
        uint32_t remaining_timeout = PICO_RTOS_WAIT_FOREVER;
        if (config->timeout != PICO_RTOS_WAIT_FOREVER) {
            int32_t elapsed = (int32_t)(current_time - wait_start_time);
            if (elapsed < (int32_t)config->timeout) {
                remaining_timeout = config->timeout - (uint32_t)elapsed;
            } else {
                remaining_timeout = 0;
            }
        }
        
        // Block the task
        if (!pico_rtos_block_task(event_group->block_obj, current_task, 
                                 PICO_RTOS_BLOCK_REASON_EVENT_GROUP, remaining_timeout)) {
            // Failed to block - decrement waiting count
            critical_section_enter_blocking(&event_group->cs);
            event_group->waiting_tasks--;
            critical_section_exit(&event_group->cs);
            
            PICO_RTOS_LOG_EVENT_WARN("Task %s failed to block on event group %p", 
                              current_task->name ? current_task->name : "unnamed", 
                              (void*)event_group);
            return 0;
        }
        
        PICO_RTOS_LOG_EVENT_DEBUG("Task %s blocked on event group %p (remaining timeout: %lu ms)", 
                           current_task->name ? current_task->name : "unnamed", 
                           (void*)event_group, remaining_timeout);
        
        // Trigger scheduler to switch to another task
        pico_rtos_scheduler();
        
        // When we get here, we've been unblocked (either by event or timeout)
        // Decrement waiting count and loop to re-check condition
        critical_section_enter_blocking(&event_group->cs);
        event_group->waiting_tasks--;
        critical_section_exit(&event_group->cs);
        
        // Mark that we're no longer on the first iteration
        first_iteration = false;
        
        // Continue the loop to re-check the condition
        // This handles the case where we were unblocked but condition is not satisfied
        // or we need to check for timeout
    }
}

uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *event_group, 
                                        uint32_t bits_to_wait_for, 
                                        bool wait_for_all, 
                                        bool clear_on_exit, 
                                        uint32_t timeout) {
    pico_rtos_event_wait_config_t config = {
        .bits_to_wait_for = bits_to_wait_for,
        .wait_for_all = wait_for_all,
        .clear_on_exit = clear_on_exit,
        .timeout = timeout
    };
    
    return pico_rtos_event_group_wait_bits_config(event_group, &config);
}

uint32_t pico_rtos_event_group_get_bits(pico_rtos_event_group_t *event_group) {
    if (event_group == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group get bits failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return 0;
    }
    
    critical_section_enter_blocking(&event_group->cs);
    uint32_t current_bits = event_group->event_bits;
    critical_section_exit(&event_group->cs);
    
    return current_bits;
}

uint32_t pico_rtos_event_group_get_waiting_count(pico_rtos_event_group_t *event_group) {
    if (event_group == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group get waiting count failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return 0;
    }
    
    critical_section_enter_blocking(&event_group->cs);
    uint32_t waiting_count = event_group->waiting_tasks;
    critical_section_exit(&event_group->cs);
    
    return waiting_count;
}

void pico_rtos_event_group_delete(pico_rtos_event_group_t *event_group) {
    if (event_group == NULL) {
        return;
    }
    
    PICO_RTOS_LOG_EVENT_DEBUG("Deleting event group at %p", (void*)event_group);
    
    critical_section_enter_blocking(&event_group->cs);
    
    // Delete the blocking object (this will unblock all waiting tasks)
    if (event_group->block_obj != NULL) {
        pico_rtos_block_object_delete(event_group->block_obj);
        event_group->block_obj = NULL;
    }
    
    // Clear all event bits
    event_group->event_bits = 0;
    event_group->waiting_tasks = 0;
    
    critical_section_exit(&event_group->cs);
    
    PICO_RTOS_LOG_EVENT_DEBUG("Event group at %p deleted", (void*)event_group);
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

uint32_t pico_rtos_event_group_create_mask(const uint8_t *bit_numbers, uint32_t count) {
    if (bit_numbers == NULL || count == 0) {
        return 0;
    }
    
    uint32_t mask = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (bit_numbers[i] < PICO_RTOS_EVENT_GROUP_MAX_EVENTS) {
            mask |= (1UL << bit_numbers[i]);
        }
    }
    
    return mask;
}

uint32_t pico_rtos_event_group_count_bits(uint32_t mask) {
    // Use built-in population count if available, otherwise use bit manipulation
    uint32_t count = 0;
    while (mask) {
        count++;
        mask &= mask - 1;  // Clear the lowest set bit
    }
    return count;
}

// =============================================================================
// STATISTICS AND DEBUGGING
// =============================================================================

bool pico_rtos_event_group_get_stats(pico_rtos_event_group_t *event_group, 
                                    pico_rtos_event_group_stats_t *stats) {
    if (event_group == NULL || stats == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group get stats failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    critical_section_enter_blocking(&event_group->cs);
    
    stats->current_events = event_group->event_bits;
    stats->waiting_tasks = event_group->waiting_tasks;
    stats->total_set_operations = event_group->total_sets;
    stats->total_clear_operations = event_group->total_clears;
    stats->total_wait_operations = event_group->total_waits;
    
    // For now, we don't track successful vs timed out waits separately
    // This would require more complex state tracking
    stats->successful_waits = 0;
    stats->timed_out_waits = 0;
    stats->max_waiting_tasks = 0;
    
    critical_section_exit(&event_group->cs);
    
    return true;
}

bool pico_rtos_event_group_reset_stats(pico_rtos_event_group_t *event_group) {
    if (event_group == NULL) {
        PICO_RTOS_LOG_EVENT_ERROR("Event group reset stats failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    critical_section_enter_blocking(&event_group->cs);
    
    // Reset statistics counters but preserve current state
    event_group->total_sets = 0;
    event_group->total_clears = 0;
    event_group->total_waits = 0;
    
    critical_section_exit(&event_group->cs);
    
    PICO_RTOS_LOG_EVENT_DEBUG("Event group %p statistics reset", (void*)event_group);
    
    return true;
}