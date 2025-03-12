#include "../include/pico_rtos/semaphore.h"
#include "pico/critical_section.h"

bool pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count) {
    if (semaphore == NULL || max_count == 0 || initial_count > max_count) {
        return false;
    }
    
    semaphore->count = initial_count;
    semaphore->max_count = max_count;
    critical_section_init(&semaphore->cs);
    semaphore->block_obj = NULL;
    
    return true;
}

bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore) {
    if (semaphore == NULL) {
        return false;
    }

    critical_section_enter_blocking(&semaphore->cs);

    // Check if semaphore is at max count
    if (semaphore->count == semaphore->max_count) {
        critical_section_exit(&semaphore->cs);
        return false;
    }

    // Increment semaphore count
    semaphore->count++;
    
    // Unblock any waiting tasks
    if (semaphore->block_obj != NULL) {
        extern void pico_rtos_task_resume_highest_priority(void);
        pico_rtos_task_resume_highest_priority();
    }

    critical_section_exit(&semaphore->cs);
    return true;
}

bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout) {
    if (semaphore == NULL) {
        return false;
    }

    critical_section_enter_blocking(&semaphore->cs);

    // Check if semaphore is available
    if (semaphore->count > 0) {
        semaphore->count--;
        critical_section_exit(&semaphore->cs);
        return true;
    }
    
    // If timeout is 0, return immediately
    if (timeout == 0) {
        critical_section_exit(&semaphore->cs);
        return false;
    }
    
    // Block the current task
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task != NULL) {
        current_task->state = PICO_RTOS_TASK_STATE_BLOCKED;
        current_task->block_reason = PICO_RTOS_BLOCK_REASON_SEMAPHORE;
        current_task->blocking_object = semaphore->block_obj;
        
        if (timeout != PICO_RTOS_WAIT_FOREVER) {
            current_task->delay_until = pico_rtos_get_tick_count() + timeout;
        }
        
        critical_section_exit(&semaphore->cs);
        
        // Trigger scheduler to switch to another task
        extern void pico_rtos_scheduler(void);
        pico_rtos_scheduler();
        
        // When we return, check if we're still blocked (timed out) or ready to take
        if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
            return false;  // Timed out
        }
        
        // We were unblocked by a give operation, so we should have a token
        return true;
    }
    
    critical_section_exit(&semaphore->cs);
    return false;
}

bool pico_rtos_semaphore_is_available(pico_rtos_semaphore_t *semaphore) {
    if (semaphore == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&semaphore->cs);
    bool available = (semaphore->count > 0);
    critical_section_exit(&semaphore->cs);
    
    return available;
}

void pico_rtos_semaphore_delete(pico_rtos_semaphore_t *semaphore) {
    if (semaphore == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&semaphore->cs);
    
    // Release any waiting tasks
    if (semaphore->block_obj != NULL) {
        // Unblock waiting tasks
        extern void pico_rtos_task_resume_highest_priority(void);
        pico_rtos_task_resume_highest_priority();
    }
    
    critical_section_exit(&semaphore->cs);
}