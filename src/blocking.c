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
    critical_section_init(&block_obj->cs);
    
    return block_obj;
}

// Delete a blocking object
void pico_rtos_block_object_delete(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    // Unblock all waiting tasks
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
    
    critical_section_exit(&block_obj->cs);
    pico_rtos_free(block_obj, sizeof(pico_rtos_block_object_t));
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
    blocked_task->next = NULL;
    
    // Add to the tail of the blocked tasks list
    if (block_obj->blocked_tasks_tail == NULL) {
        block_obj->blocked_tasks_head = blocked_task;
        block_obj->blocked_tasks_tail = blocked_task;
    } else {
        block_obj->blocked_tasks_tail->next = blocked_task;
        block_obj->blocked_tasks_tail = blocked_task;
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

// Unblock the highest priority task from the blocking object
pico_rtos_task_t *pico_rtos_unblock_highest_priority_task(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL || block_obj->blocked_count == 0) {
        return NULL;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    pico_rtos_task_t *highest_priority_task = NULL;
    pico_rtos_blocked_task_t *highest_priority_blocked = NULL;
    pico_rtos_blocked_task_t *prev_highest = NULL;
    pico_rtos_blocked_task_t *prev = NULL;
    pico_rtos_blocked_task_t *current = block_obj->blocked_tasks_head;
    
    // Find the highest priority task in the blocked list
    while (current != NULL) {
        if (current->task != NULL && 
            (highest_priority_task == NULL || 
             current->task->priority > highest_priority_task->priority)) {
            highest_priority_task = current->task;
            highest_priority_blocked = current;
            prev_highest = prev;
        }
        prev = current;
        current = current->next;
    }
    
    if (highest_priority_blocked != NULL) {
        // Remove from blocked list
        if (prev_highest == NULL) {
            // Removing head
            block_obj->blocked_tasks_head = highest_priority_blocked->next;
        } else {
            prev_highest->next = highest_priority_blocked->next;
        }
        
        if (highest_priority_blocked == block_obj->blocked_tasks_tail) {
            // Removing tail
            block_obj->blocked_tasks_tail = prev_highest;
        }
        
        block_obj->blocked_count--;
        
        // Update task state
        highest_priority_task->state = PICO_RTOS_TASK_STATE_READY;
        highest_priority_task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
        highest_priority_task->blocking_object = NULL;
        
        pico_rtos_free(highest_priority_blocked, sizeof(pico_rtos_blocked_task_t));
    }
    
    critical_section_exit(&block_obj->cs);
    return highest_priority_task;
}

// Check for timed out tasks and unblock them
void pico_rtos_check_blocked_timeouts(pico_rtos_block_object_t *block_obj) {
    if (block_obj == NULL || block_obj->blocked_count == 0) {
        return;
    }
    
    critical_section_enter_blocking(&block_obj->cs);
    
    uint32_t current_time = pico_rtos_get_tick_count();
    pico_rtos_blocked_task_t *prev = NULL;
    pico_rtos_blocked_task_t *current = block_obj->blocked_tasks_head;
    
    while (current != NULL) {
        pico_rtos_blocked_task_t *next = current->next;
        
        // Check if this task has timed out (overflow-safe comparison)
        if (current->timeout_time != UINT32_MAX && 
            (int32_t)(current_time - current->timeout_time) >= 0) {
            // Remove from blocked list
            if (prev == NULL) {
                block_obj->blocked_tasks_head = next;
            } else {
                prev->next = next;
            }
            
            if (current == block_obj->blocked_tasks_tail) {
                block_obj->blocked_tasks_tail = prev;
            }
            
            block_obj->blocked_count--;
            
            // Update task state (keep block reason to indicate timeout)
            if (current->task != NULL) {
                current->task->state = PICO_RTOS_TASK_STATE_READY;
                // Note: We keep the block_reason to indicate the operation timed out
                current->task->blocking_object = NULL;
            }
            
            pico_rtos_free(current, sizeof(pico_rtos_blocked_task_t));
        } else {
            prev = current;
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