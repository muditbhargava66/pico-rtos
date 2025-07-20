#include "pico_rtos/queue.h"
#include "pico_rtos/error.h"
#include "pico_rtos.h"
#include "pico/critical_section.h"
#include <string.h>

bool pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, size_t item_size, size_t max_items) {
    if (queue == NULL || buffer == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (item_size == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_QUEUE_INVALID_ITEM_SIZE, item_size);
        return false;
    }
    
    if (max_items == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_QUEUE_INVALID_SIZE, max_items);
        return false;
    }
    
    queue->buffer = buffer;
    queue->item_size = item_size;
    queue->max_items = max_items;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    
    critical_section_init(&queue->cs);
    
    queue->send_block_obj = NULL;
    queue->receive_block_obj = NULL;
    
    return true;
}

bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout) {
    if (queue == NULL || item == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }

    critical_section_enter_blocking(&queue->cs);

    // Check if queue is full
    if (queue->count == queue->max_items) {
        // If timeout is 0, return immediately
        if (timeout == 0) {
            critical_section_exit(&queue->cs);
            return false;
        }
        
        // Block the current task
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (current_task != NULL) {
            current_task->state = PICO_RTOS_TASK_STATE_BLOCKED;
            current_task->block_reason = PICO_RTOS_BLOCK_REASON_QUEUE_FULL;
            current_task->blocking_object = queue->send_block_obj;
            
            if (timeout != PICO_RTOS_WAIT_FOREVER) {
                current_task->delay_until = pico_rtos_get_tick_count() + timeout;
            }
            
            critical_section_exit(&queue->cs);
            
            // Trigger scheduler to switch to another task
            extern void pico_rtos_scheduler(void);
            pico_rtos_scheduler();
            
            // When we return, check if we're still blocked (timed out) or ready to send
            if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
                return false;  // Timed out
            }
            
            // Reacquire lock to continue sending
            critical_section_enter_blocking(&queue->cs);
        }
    }

    // At this point, we should have space in the queue
    if (queue->count < queue->max_items) {
        // Copy item to queue
        uint8_t *dest = (uint8_t *)queue->buffer + queue->tail * queue->item_size;
        memcpy(dest, item, queue->item_size);
        
        // Update tail and count
        queue->tail = (queue->tail + 1) % queue->max_items;
        queue->count++;
        
        // Unblock any tasks waiting to receive
        if (queue->receive_block_obj != NULL) {
            extern void pico_rtos_task_resume_highest_priority(void);
            pico_rtos_task_resume_highest_priority();
        }
        
        critical_section_exit(&queue->cs);
        return true;
    }
    
    // This should not happen if the blocking mechanism works correctly
    critical_section_exit(&queue->cs);
    return false;
}

bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout) {
    if (queue == NULL || item == NULL) {
        return false;
    }

    critical_section_enter_blocking(&queue->cs);

    // Check if queue is empty
    if (queue->count == 0) {
        // If timeout is 0, return immediately
        if (timeout == 0) {
            critical_section_exit(&queue->cs);
            return false;
        }
        
        // Block the current task
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (current_task != NULL) {
            current_task->state = PICO_RTOS_TASK_STATE_BLOCKED;
            current_task->block_reason = PICO_RTOS_BLOCK_REASON_QUEUE_EMPTY;
            current_task->blocking_object = queue->receive_block_obj;
            
            if (timeout != PICO_RTOS_WAIT_FOREVER) {
                current_task->delay_until = pico_rtos_get_tick_count() + timeout;
            }
            
            critical_section_exit(&queue->cs);
            
            // Trigger scheduler to switch to another task
            extern void pico_rtos_scheduler(void);
            pico_rtos_scheduler();
            
            // When we return, check if we're still blocked (timed out) or ready to receive
            if (current_task->block_reason != PICO_RTOS_BLOCK_REASON_NONE) {
                return false;  // Timed out
            }
            
            // Reacquire lock to continue receiving
            critical_section_enter_blocking(&queue->cs);
        }
    }

    // At this point, we should have an item in the queue
    if (queue->count > 0) {
        // Copy item from queue
        uint8_t *src = (uint8_t *)queue->buffer + queue->head * queue->item_size;
        memcpy(item, src, queue->item_size);
        
        // Update head and count
        queue->head = (queue->head + 1) % queue->max_items;
        queue->count--;
        
        // Unblock any tasks waiting to send
        if (queue->send_block_obj != NULL) {
            extern void pico_rtos_task_resume_highest_priority(void);
            pico_rtos_task_resume_highest_priority();
        }
        
        critical_section_exit(&queue->cs);
        return true;
    }
    
    // This should not happen if the blocking mechanism works correctly
    critical_section_exit(&queue->cs);
    return false;
}

bool pico_rtos_queue_is_empty(pico_rtos_queue_t *queue) {
    if (queue == NULL) {
        return true;
    }
    
    critical_section_enter_blocking(&queue->cs);
    bool is_empty = (queue->count == 0);
    critical_section_exit(&queue->cs);
    
    return is_empty;
}

bool pico_rtos_queue_is_full(pico_rtos_queue_t *queue) {
    if (queue == NULL) {
        return true;
    }
    
    critical_section_enter_blocking(&queue->cs);
    bool is_full = (queue->count == queue->max_items);
    critical_section_exit(&queue->cs);
    
    return is_full;
}

void pico_rtos_queue_delete(pico_rtos_queue_t *queue) {
    if (queue == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&queue->cs);
    
    // Release any waiting tasks
    if (queue->send_block_obj != NULL || queue->receive_block_obj != NULL) {
        // Unblock waiting tasks
        extern void pico_rtos_task_resume_highest_priority(void);
        pico_rtos_task_resume_highest_priority();
    }
    
    queue->count = 0;
    
    critical_section_exit(&queue->cs);
}