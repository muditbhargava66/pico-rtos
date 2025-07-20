#include "pico_rtos/blocking.h"
#include "pico_rtos/task.h"
#include "pico_rtos.h"
#include "pico/critical_section.h"
#include <stdlib.h>
#include <string.h>

// Create a new blocking object
pico_rtos_block_object_t *pico_rtos_block_object_create(void *sync_object) {
    pico_rtos_block_object_t *block_obj = (pico_rtos_block_object_t *)pico_rtos_malloc(sizeof(pico_rtos_block_object_t));
    if (block_obj == NULL) {
        return NULL;
    }
    
    block_obj->sync_object = sync_object;
    block_obj->blocked_tasks_head = NULL;
    block_obj->blocked_tasks_tail = NULL;
    block_obj->blocked_count = 0;
    block_obj->priority_ordered = true;  // Enable priority ordering for real-time performance
    critical_section_init(&block_obj->cs);
    
    return block_obj;
}

// Delete a blocking object
void pico_rtos_block_object_delete(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    // Unblock all waiting tasks - O(n) but only during cleanup
    pico_rtos_blocked_task_t *blocked_task = block_obj->blocked_tasks_head;
    while (blocked_task != NULL) {
        pico_rtos_blocked_task_t *next = blocked_task->next;
        
        // Mark task as ready and clear blocking reason
        if (blocked_task->task != NULL) {
            blocked_task->task->state = PICO_RTOS_TASK_STATE_READY;
            blocked_task->task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
            blocked_task->task->blocking_object = NULL;
        }
        
        pico_rtos_free(blocked_task, sizeof(pico_rtos_blocked_task_t));
        blocked_task = next;
    }
    
    block_obj->blocked_tasks_head = NULL;
    block_obj->blocked_tasks_tail = NULL;
    block_obj->blocked_count = 0;
    
    critical_section_exit(&block_obj->cs);
    pico_rtos_free(block_obj, sizeof(pico_rtos_block_object_t));
}

// PERFORMANCE CRITICAL: Insert task in priority order for O(1) unblocking
static void insert_blocked_task_priority_ordered(pico_rtos_block_object_t *block_obj, 
                                               pico_rtos_blocked_task_t *new_task) {
    // Find insertion point - highest priority tasks at head
    pico_rtos_blocked_task_t *current = block_obj->blocked_tasks_head;
    pico_rtos_blocked_task_t *prev = NULL;
    
    // Traverse until we find a task with lower or equal priority
    while (current != NULL && current->priority > new_task->priority) {
        prev = current;
        current = current->next;
    }
    
    // Insert new task at found position - O(1) insertion
    new_task->next = current;
    new_task->prev = prev;
    
    if (prev == NULL) {
        // Inserting at head (highest priority)
        block_obj->blocked_tasks_head = new_task;
    } else {
        prev->next = new_task;
    }
    
    if (current == NULL) {
        // Inserting at tail (lowest priority)
        block_obj->blocked_tasks_tail = new_task;
    } else {
        current->prev = new_task;
    }
}

// Add a task to the blocking object's wait list
bool pico_rtos_block_task(pico_rtos_block_object_t *block_obj, pico_rtos_task_t *task, 
                         pico_rtos_block_reason_t reason, uint32_t timeout) {
    if (block_obj == NULL || task == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    // Create a new blocked task entry
    pico_rtos_blocked_task_t *blocked_task = (pico_rtos_blocked_task_t *)pico_rtos_malloc(sizeof(pico_rtos_blocked_task_t));
    if (blocked_task == NULL) {
        critical_section_exit(&block_obj->cs);
        return false;
    }
    
    blocked_task->task = task;
    blocked_task->timeout_time = (timeout == PICO_RTOS_WAIT_FOREVER) ? 
                                UINT32_MAX : (pico_rtos_get_tick_count() + timeout);
    blocked_task->priority = task->priority;  // Cache priority for fast access
    blocked_task->next = NULL;
    blocked_task->prev = NULL;
    
    // PERFORMANCE CRITICAL: Use priority-ordered insertion for O(1) unblocking
    if (block_obj->priority_ordered) {
        insert_blocked_task_priority_ordered(block_obj, blocked_task);
    } else {
        // Fallback: Add to tail (FIFO order) - for compatibility
        if (block_obj->blocked_tasks_tail == NULL) {
            block_obj->blocked_tasks_head = blocked_task;
            block_obj->blocked_tasks_tail = blocked_task;
        } else {
            block_obj->blocked_tasks_tail->next = blocked_task;
            blocked_task->prev = block_obj->blocked_tasks_tail;
            block_obj->blocked_tasks_tail = blocked_task;
        }
    }
    
    block_obj->blocked_count++;
    
    // Update task state
    task->state = PICO_RTOS_TASK_STATE_BLOCKED;
    task->block_reason = reason;
    task->blocking_object = block_obj;
    task->delay_until = blocked_task->timeout_time;
    
    critical_section_exit(&block_obj->cs);
    return true;
}

// PERFORMANCE CRITICAL: O(1) unblocking for priority-ordered lists
static pico_rtos_task_t *unblock_highest_priority_task_fast(pico_rtos_block_object_t *block_obj) {
    if (block_obj->blocked_tasks_head == NULL) {
        return NULL;
    }
    
    // Highest priority task is always at head - O(1) access
    pico_rtos_blocked_task_t *highest = block_obj->blocked_tasks_head;
    pico_rtos_task_t *task = highest->task;
    
    // Remove from head - O(1) operation
    block_obj->blocked_tasks_head = highest->next;
    if (block_obj->blocked_tasks_head != NULL) {
        block_obj->blocked_tasks_head->prev = NULL;
    } else {
        block_obj->blocked_tasks_tail = NULL;
    }
    
    block_obj->blocked_count--;
    
    // Update task state
    if (task != NULL) {
        task->state = PICO_RTOS_TASK_STATE_READY;
        task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
        task->blocking_object = NULL;
    }
    
    pico_rtos_free(highest, sizeof(pico_rtos_blocked_task_t));
    return task;
}

// LEGACY: O(n) unblocking for non-priority-ordered lists (compatibility)
static pico_rtos_task_t *unblock_highest_priority_task_legacy(pico_rtos_block_object_t *block_obj) {
    pico_rtos_task_t *highest_priority_task = NULL;
    pico_rtos_blocked_task_t *highest_priority_blocked = NULL;
    pico_rtos_blocked_task_t *prev_highest = NULL;
    pico_rtos_blocked_task_t *prev = NULL;
    pico_rtos_blocked_task_t *current = block_obj->blocked_tasks_head;
    
    // Find the highest priority task in the blocked list - O(n)
    while (current != NULL) {
        if (current->task != NULL && 
            (highest_priority_task == NULL || 
             current->priority > highest_priority_task->priority)) {
            highest_priority_task = current->task;
            highest_priority_blocked = current;
            prev_highest = prev;
        }
        prev = current;
        current = current->next;
    }
    
    if (highest_priority_blocked != NULL) {
        // Remove from blocked list - O(1) with doubly-linked list
        if (highest_priority_blocked->prev != NULL) {
            highest_priority_blocked->prev->next = highest_priority_blocked->next;
        } else {
            block_obj->blocked_tasks_head = highest_priority_blocked->next;
        }
        
        if (highest_priority_blocked->next != NULL) {
            highest_priority_blocked->next->prev = highest_priority_blocked->prev;
        } else {
            block_obj->blocked_tasks_tail = highest_priority_blocked->prev;
        }
        
        block_obj->blocked_count--;
        
        // Update task state
        if (highest_priority_task != NULL) {
            highest_priority_task->state = PICO_RTOS_TASK_STATE_READY;
            highest_priority_task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
            highest_priority_task->blocking_object = NULL;
        }
        
        pico_rtos_free(highest_priority_blocked, sizeof(pico_rtos_blocked_task_t));
    }
    
    return highest_priority_task;
}

// Unblock the highest priority task from the blocking object
pico_rtos_task_t *pico_rtos_unblock_highest_priority_task(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL || block_obj->blocked_count == 0) {
        return NULL;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    pico_rtos_task_t *unblocked_task = NULL;
    
    // PERFORMANCE CRITICAL: Use O(1) unblocking for priority-ordered lists
    if (block_obj->priority_ordered) {
        unblocked_task = unblock_highest_priority_task_fast(block_obj);
    } else {
        // Fallback to O(n) for compatibility with non-ordered lists
        unblocked_task = unblock_highest_priority_task_legacy(block_obj);
    }
    
    critical_section_exit(&block_obj->cs);
    return unblocked_task;
}

// Check for timed out tasks and unblock them
void pico_rtos_check_blocked_timeouts(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL || block_obj->blocked_count == 0) {
        return;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    uint32_t current_time = pico_rtos_get_tick_count();
    pico_rtos_blocked_task_t *current = block_obj->blocked_tasks_head;
    
    while (current != NULL) {
        pico_rtos_blocked_task_t *next = current->next;
        
        // Check if this task has timed out (overflow-safe comparison)
        if (current->timeout_time != UINT32_MAX && 
            (int32_t)(current_time - current->timeout_time) >= 0) {
            
            // PERFORMANCE OPTIMIZED: Use doubly-linked list for O(1) removal
            if (current->prev != NULL) {
                current->prev->next = current->next;
            } else {
                block_obj->blocked_tasks_head = current->next;
            }
            
            if (current->next != NULL) {
                current->next->prev = current->prev;
            } else {
                block_obj->blocked_tasks_tail = current->prev;
            }
            
            block_obj->blocked_count--;
            
            // Update task state (keep block reason to indicate timeout)
            if (current->task != NULL) {
                current->task->state = PICO_RTOS_TASK_STATE_READY;
                // Note: We keep the block_reason to indicate the operation timed out
                current->task->blocking_object = NULL;
            }
            
            pico_rtos_free(current, sizeof(pico_rtos_blocked_task_t));
        }
        
        current = next;
    }
    
    critical_section_exit(&block_obj->cs);
}

// Get the number of blocked tasks
uint32_t pico_rtos_get_blocked_count(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    uint32_t count = block_obj->blocked_count;
    critical_section_exit(&block_obj->cs);
    
    return count;
}

// PERFORMANCE MONITORING: Get blocking system performance statistics

void pico_rtos_get_blocking_stats(pico_rtos_block_object_t *block_obj, pico_rtos_blocking_stats_t *stats) {
    if (block_obj == NULL || stats == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    stats->total_blocked_tasks = block_obj->blocked_count;
    stats->priority_ordered = block_obj->priority_ordered;
    
    // Calculate maximum blocked tasks by traversing list
    uint32_t max_priority = 0;
    pico_rtos_blocked_task_t *current = block_obj->blocked_tasks_head;
    while (current != NULL) {
        if (current->priority > max_priority) {
            max_priority = current->priority;
        }
        current = current->next;
    }
    stats->max_blocked_tasks = max_priority;
    
    critical_section_exit(&block_obj->cs);
}

// PERFORMANCE VALIDATION: Verify priority ordering
bool pico_rtos_validate_priority_ordering(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL || !block_obj->priority_ordered) {
        return true;  // Not using priority ordering
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    bool valid = true;
    uint32_t prev_priority = UINT32_MAX;
    pico_rtos_blocked_task_t *current = block_obj->blocked_tasks_head;
    
    while (current != NULL && valid) {
        if (current->priority > prev_priority) {
            valid = false;  // Priority ordering violated
        }
        prev_priority = current->priority;
        current = current->next;
    }
    
    critical_section_exit(&block_obj->cs);
    return valid;
}