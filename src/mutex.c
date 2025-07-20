#include "pico_rtos/mutex.h"
#include "pico_rtos/blocking.h"
#include "pico_rtos/error.h"
#include "pico_rtos/logging.h"
#include "pico/critical_section.h"

bool pico_rtos_mutex_init(pico_rtos_mutex_t *mutex) {
    if (mutex == NULL) {
        PICO_RTOS_LOG_MUTEX_ERROR("Mutex initialization failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    PICO_RTOS_LOG_MUTEX_DEBUG("Initializing mutex at %p", (void*)mutex);
    
    mutex->owner = NULL;
    mutex->lock_count = 0;
    critical_section_init(&mutex->cs);
    
    // Create blocking object for this mutex
    mutex->block_obj = pico_rtos_block_object_create(mutex);
    if (mutex->block_obj == NULL) {
        PICO_RTOS_LOG_MUTEX_ERROR("Mutex initialization failed: Could not create blocking object");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_OUT_OF_MEMORY, 0);
        return false;
    }
    
    PICO_RTOS_LOG_MUTEX_DEBUG("Mutex at %p initialized successfully", (void*)mutex);
    return true;
}

bool pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex, uint32_t timeout) {
    if (mutex == NULL) {
        PICO_RTOS_LOG_MUTEX_ERROR("Mutex lock failed: NULL pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task == NULL) {
        PICO_RTOS_LOG_MUTEX_ERROR("Mutex lock failed: Called from interrupt context");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INTERRUPT_CONTEXT_VIOLATION, 0);
        return false;
    }

    PICO_RTOS_LOG_MUTEX_DEBUG("Task %s attempting to lock mutex %p (timeout %lu)", 
                             current_task->name ? current_task->name : "unnamed", 
                             (void*)mutex, timeout);

    critical_section_enter_blocking(&mutex->cs);

    // Case 1: Mutex is available
    if (mutex->owner == NULL) {
        mutex->owner = current_task;
        mutex->lock_count = 1;
        critical_section_exit(&mutex->cs);
        PICO_RTOS_LOG_MUTEX_DEBUG("Task %s acquired mutex %p", 
                                 current_task->name ? current_task->name : "unnamed", 
                                 (void*)mutex);
        return true;
    }
    
    // Case 2: Current task already owns the mutex (recursive lock)
    if (mutex->owner == current_task) {
        mutex->lock_count++;
        critical_section_exit(&mutex->cs);
        PICO_RTOS_LOG_MUTEX_DEBUG("Task %s recursively locked mutex %p (count %lu)", 
                                 current_task->name ? current_task->name : "unnamed", 
                                 (void*)mutex, mutex->lock_count);
        return true;
    }
    
    // Case 3: Mutex is owned by another task
    PICO_RTOS_LOG_MUTEX_DEBUG("Mutex %p owned by task %s, task %s will block", 
                             (void*)mutex,
                             mutex->owner->name ? mutex->owner->name : "unnamed",
                             current_task->name ? current_task->name : "unnamed");
    
    // Implement priority inheritance - boost owner's priority if needed
    if (current_task->priority > mutex->owner->priority) {
        PICO_RTOS_LOG_MUTEX_DEBUG("Priority inheritance: boosting task %s priority from %lu to %lu", 
                                 mutex->owner->name ? mutex->owner->name : "unnamed",
                                 mutex->owner->priority, current_task->priority);
        mutex->owner->priority = current_task->priority;
    }
    
    // If timeout is zero, don't wait
    if (timeout == 0) {
        critical_section_exit(&mutex->cs);
        PICO_RTOS_LOG_MUTEX_DEBUG("Task %s failed to acquire mutex %p (no wait)", 
                                 current_task->name ? current_task->name : "unnamed", 
                                 (void*)mutex);
        return false;
    }
    
    // Block task until mutex becomes available or timeout
    critical_section_exit(&mutex->cs);
    
    // Use the blocking mechanism to block the task
    if (!pico_rtos_block_task(mutex->block_obj, current_task, PICO_RTOS_BLOCK_REASON_MUTEX, timeout)) {
        PICO_RTOS_LOG_MUTEX_WARN("Task %s failed to block on mutex %p", 
                                current_task->name ? current_task->name : "unnamed", 
                                (void*)mutex);
        return false;
    }
    
    // Trigger scheduler to switch to another task
    extern void pico_rtos_scheduler(void);
    pico_rtos_scheduler();
    
    // When we get here, we've been unblocked
    // Check if we got the mutex or timed out
    critical_section_enter_blocking(&mutex->cs);
    bool success = (mutex->owner == current_task);
    critical_section_exit(&mutex->cs);
    
    if (success) {
        PICO_RTOS_LOG_MUTEX_DEBUG("Task %s acquired mutex %p after blocking", 
                                 current_task->name ? current_task->name : "unnamed", 
                                 (void*)mutex);
    } else {
        PICO_RTOS_LOG_MUTEX_WARN("Task %s timed out waiting for mutex %p", 
                                current_task->name ? current_task->name : "unnamed", 
                                (void*)mutex);
    }
    
    return success;
}

bool pico_rtos_mutex_try_lock(pico_rtos_mutex_t *mutex) {
    return pico_rtos_mutex_lock(mutex, 0);
}

bool pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex) {
    if (mutex == NULL) {
        PICO_RTOS_LOG_MUTEX_ERROR("Mutex unlock failed: NULL pointer");
        return false;
    }
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task == NULL) {
        PICO_RTOS_LOG_MUTEX_ERROR("Mutex unlock failed: Called from interrupt context");
        return false;
    }

    critical_section_enter_blocking(&mutex->cs);

    // Check if the current task owns the mutex
    if (mutex->owner != current_task) {
        critical_section_exit(&mutex->cs);
        PICO_RTOS_LOG_MUTEX_ERROR("Task %s attempted to unlock mutex %p owned by task %s", 
                                 current_task->name ? current_task->name : "unnamed",
                                 (void*)mutex,
                                 mutex->owner ? (mutex->owner->name ? mutex->owner->name : "unnamed") : "none");
        return false;
    }
    
    PICO_RTOS_LOG_MUTEX_DEBUG("Task %s unlocking mutex %p (count %lu)", 
                             current_task->name ? current_task->name : "unnamed", 
                             (void*)mutex, mutex->lock_count);
    
    // Decrement lock count, release mutex if count reaches 0
    mutex->lock_count--;
    if (mutex->lock_count == 0) {
        // Restore original priority (priority inheritance cleanup)
        if (current_task->priority != current_task->original_priority) {
            PICO_RTOS_LOG_MUTEX_DEBUG("Restoring task %s priority from %lu to %lu", 
                                     current_task->name ? current_task->name : "unnamed",
                                     current_task->priority, current_task->original_priority);
            current_task->priority = current_task->original_priority;
        }
        
        // Unblock the highest priority waiting task and give it the mutex
        pico_rtos_task_t *unblocked_task = pico_rtos_unblock_highest_priority_task(mutex->block_obj);
        if (unblocked_task != NULL) {
            // Give the mutex to the unblocked task
            mutex->owner = unblocked_task;
            mutex->lock_count = 1;
            PICO_RTOS_LOG_MUTEX_DEBUG("Mutex %p transferred to task %s", 
                                     (void*)mutex,
                                     unblocked_task->name ? unblocked_task->name : "unnamed");
        } else {
            // No tasks waiting, release the mutex
            mutex->owner = NULL;
            PICO_RTOS_LOG_MUTEX_DEBUG("Mutex %p released (no waiting tasks)", (void*)mutex);
        }
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
    
    // Delete the blocking object (this will unblock all waiting tasks)
    if (mutex->block_obj != NULL) {
        pico_rtos_block_object_delete(mutex->block_obj);
        mutex->block_obj = NULL;
    }
    
    mutex->owner = NULL;
    mutex->lock_count = 0;
    
    critical_section_exit(&mutex->cs);
}