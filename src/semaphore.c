#include "pico_rtos/semaphore.h"
#include "pico_rtos/blocking.h"
#include "pico_rtos/error.h"
#include "pico_rtos.h"
#include "pico/critical_section.h"

bool pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count) {
    if (semaphore == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (max_count == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_SEMAPHORE_INVALID_COUNT, max_count);
        return false;
    }
    
    if (initial_count > max_count) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_SEMAPHORE_INVALID_COUNT, initial_count);
        return false;
    }
    
    semaphore->count = initial_count;
    semaphore->max_count = max_count;
    critical_section_init(&semaphore->cs);
    
    // CRITICAL FIX: Properly initialize blocking object
    extern pico_rtos_block_object_t *pico_rtos_block_object_create(void *sync_object);
    semaphore->block_obj = pico_rtos_block_object_create(semaphore);
    if (semaphore->block_obj == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_OUT_OF_MEMORY, 0);
        return false;
    }
    
    return true;
}

bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore) {
    if (semaphore == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }

    critical_section_enter_blocking(&semaphore->cs);

    // Check if semaphore is at max count
    if (semaphore->count == semaphore->max_count) {
        critical_section_exit(&semaphore->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_SEMAPHORE_OVERFLOW, semaphore->max_count);
        return false;
    }

    // CRITICAL FIX: Check for blocked tasks BEFORE incrementing count
    // This ensures we don't waste tokens when tasks are waiting
    extern pico_rtos_task_t *pico_rtos_unblock_highest_priority_task(pico_rtos_block_object_t *block_obj);
    pico_rtos_task_t *unblocked_task = NULL;
    
    if (semaphore->block_obj != NULL) {
        unblocked_task = pico_rtos_unblock_highest_priority_task(semaphore->block_obj);
    }
    
    if (unblocked_task == NULL) {
        // No blocked tasks, increment semaphore count
        semaphore->count++;
    }
    // If we unblocked a task, don't increment count - the task gets the token directly

    critical_section_exit(&semaphore->cs);
    
    // If we unblocked a task, trigger scheduler
    if (unblocked_task != NULL) {
        extern void pico_rtos_schedule_next_task(void);
        pico_rtos_schedule_next_task();
    }
    
    return true;
}

bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout) {
    if (semaphore == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
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
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_SEMAPHORE_TIMEOUT, 0);
        return false;
    }
    
    // CRITICAL FIX: Use proper blocking mechanism
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task != NULL && semaphore->block_obj != NULL) {
        critical_section_exit(&semaphore->cs);
        
        // Block the task using the proper blocking mechanism
        extern bool pico_rtos_block_task(pico_rtos_block_object_t *block_obj, pico_rtos_task_t *task, 
                                        pico_rtos_block_reason_t reason, uint32_t timeout);
        
        if (!pico_rtos_block_task(semaphore->block_obj, current_task, PICO_RTOS_BLOCK_REASON_SEMAPHORE, timeout)) {
            PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_OUT_OF_MEMORY, 0);
            return false;
        }
        
        // Trigger scheduler to switch to another task
        extern void pico_rtos_schedule_next_task(void);
        pico_rtos_schedule_next_task();
        
        // When we return, check if we're still blocked (timed out) or unblocked
        if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
            PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_SEMAPHORE_TIMEOUT, timeout);
            return false;  // Timed out
        }
        
        // We were unblocked by a give operation - token was given directly to us
        return true;
    }
    
    critical_section_exit(&semaphore->cs);
    PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_SYSTEM_NOT_INITIALIZED, 0);
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
    
    // CRITICAL FIX: Properly delete blocking object and unblock all waiting tasks
    if (semaphore->block_obj != NULL) {
        extern void pico_rtos_block_object_delete(pico_rtos_block_object_t *block_obj);
        pico_rtos_block_object_delete(semaphore->block_obj);
        semaphore->block_obj = NULL;
    }
    
    critical_section_exit(&semaphore->cs);
}