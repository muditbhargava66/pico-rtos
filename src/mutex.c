#include "../include/pico_rtos/mutex.h"
#include "pico/critical_section.h"

bool pico_rtos_mutex_init(pico_rtos_mutex_t *mutex) {
    if (mutex == NULL) {
        return false;
    }
    
    mutex->owner = NULL;
    mutex->lock_count = 0;
    critical_section_init(&mutex->cs);
    mutex->block_obj = NULL;
    
    return true;
}

bool pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex, uint32_t timeout) {
    if (mutex == NULL) {
        return false;
    }
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task == NULL) {
        return false;
    }

    critical_section_enter_blocking(&mutex->cs);

    // Case 1: Mutex is available
    if (mutex->owner == NULL) {
        mutex->owner = current_task;
        mutex->lock_count = 1;
        critical_section_exit(&mutex->cs);
        return true;
    }
    
    // Case 2: Current task already owns the mutex (recursive lock)
    if (mutex->owner == current_task) {
        mutex->lock_count++;
        critical_section_exit(&mutex->cs);
        return true;
    }
    
    // Case 3: Mutex is owned by another task
    // If timeout is zero, don't wait
    if (timeout == 0) {
        critical_section_exit(&mutex->cs);
        return false;
    }
    
    // Block task until mutex becomes available or timeout
    current_task->state = PICO_RTOS_TASK_STATE_BLOCKED;
    current_task->block_reason = PICO_RTOS_BLOCK_REASON_MUTEX;
    current_task->blocking_object = mutex->block_obj;
    
    if (timeout != PICO_RTOS_WAIT_FOREVER) {
        current_task->delay_until = pico_rtos_get_tick_count() + timeout;
    }
    
    critical_section_exit(&mutex->cs);
    
    // Trigger scheduler to switch to another task
    extern void pico_rtos_scheduler(void);
    pico_rtos_scheduler();
    
    // When we get here, we've been unblocked
    // Check if we got the mutex or timed out
    return (current_task->block_reason == PICO_RTOS_BLOCK_REASON_NONE);
}

bool pico_rtos_mutex_try_lock(pico_rtos_mutex_t *mutex) {
    return pico_rtos_mutex_lock(mutex, 0);
}

bool pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex) {
    if (mutex == NULL) {
        return false;
    }
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task == NULL) {
        return false;
    }

    critical_section_enter_blocking(&mutex->cs);

    // Check if the current task owns the mutex
    if (mutex->owner != current_task) {
        critical_section_exit(&mutex->cs);
        return false;
    }
    
    // Decrement lock count, release mutex if count reaches 0
    mutex->lock_count--;
    if (mutex->lock_count == 0) {
        mutex->owner = NULL;
        
        // Unblock any waiting tasks
        extern void pico_rtos_task_resume_highest_priority(void);
        pico_rtos_task_resume_highest_priority();
    }

    critical_section_exit(&mutex->cs);
    return true;
}

pico_rtos_task_t *pico_rtos_mutex_get_owner(pico_rtos_mutex_t *mutex) {
    if (mutex == NULL) {
        return NULL;
    }
    
    pico_rtos_task_t *owner;
    critical_section_enter_blocking(&mutex->cs);
    owner = mutex->owner;
    critical_section_exit(&mutex->cs);
    
    return owner;
}

void pico_rtos_mutex_delete(pico_rtos_mutex_t *mutex) {
    if (mutex == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&mutex->cs);
    
    // Release any waiting tasks
    if (mutex->block_obj != NULL) {
        // Unblock waiting tasks
        extern void pico_rtos_task_resume_highest_priority(void);
        pico_rtos_task_resume_highest_priority();
    }
    
    mutex->owner = NULL;
    mutex->lock_count = 0;
    
    critical_section_exit(&mutex->cs);
}