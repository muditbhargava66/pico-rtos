#include <stdlib.h>
#include <string.h>
#include "../include/pico_rtos/task.h"
#include "pico/critical_section.h"

// Properly allocate and initialize task stack
static bool pico_rtos_setup_task_stack(pico_rtos_task_t *task);

bool pico_rtos_task_create(pico_rtos_task_t *task, const char *name, 
                          pico_rtos_task_function_t function, void *param, 
                          uint32_t stack_size, uint32_t priority) {
    if (task == NULL || function == NULL) {
        return false;
    }
    
    // Initialize task structure
    memset(task, 0, sizeof(pico_rtos_task_t));
    task->name = name;
    task->function = function;
    task->param = param;
    task->stack_size = stack_size;
    task->priority = priority;
    task->state = PICO_RTOS_TASK_STATE_READY;
    task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
    
    // Initialize critical section for this task
    critical_section_init(&task->cs);
    
    // Setup task stack
    if (!pico_rtos_setup_task_stack(task)) {
        return false;
    }
    
    // Add task to scheduler
    pico_rtos_scheduler_add_task(task);
    
    return true;
}

void pico_rtos_task_suspend(pico_rtos_task_t *task) {
    pico_rtos_enter_critical();
    
    if (task == NULL) {
        // Suspend current task
        task = pico_rtos_get_current_task();
    }
    
    if (task != NULL) {
        task->state = PICO_RTOS_TASK_STATE_SUSPENDED;
        
        // If suspending current task, trigger a context switch
        if (task == pico_rtos_get_current_task()) {
            pico_rtos_exit_critical();
            pico_rtos_scheduler(); // Trigger scheduler
            return;
        }
    }
    
    pico_rtos_exit_critical();
}

void pico_rtos_task_resume(pico_rtos_task_t *task) {
    if (task == NULL) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    if (task->state == PICO_RTOS_TASK_STATE_SUSPENDED) {
        task->state = PICO_RTOS_TASK_STATE_READY;
    }
    
    pico_rtos_exit_critical();
}

void pico_rtos_task_delay(uint32_t ms) {
    if (ms == 0) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task != NULL) {
        current_task->state = PICO_RTOS_TASK_STATE_BLOCKED;
        current_task->block_reason = PICO_RTOS_BLOCK_REASON_DELAY;
        current_task->delay_until = pico_rtos_get_tick_count() + ms;
        
        pico_rtos_exit_critical();
        pico_rtos_scheduler(); // Trigger scheduler to switch tasks
        return;
    }
    
    pico_rtos_exit_critical();
}

// External function implemented in core.c
extern pico_rtos_task_t *pico_rtos_get_current_task(void);

// Internal function to trigger the scheduler (defined in core.c)
void pico_rtos_scheduler(void) {
    // This acts as a wrapper to call into core.c
    // In a real implementation, this would trigger a context switch
    // For now, we'll just call the scheduler function
    pico_rtos_schedule_next_task();
}

// Setup task stack - this is where architecture-specific code would go
static bool pico_rtos_setup_task_stack(pico_rtos_task_t *task) {
    // In a real implementation, this would allocate memory for the stack
    // and set up the initial stack frame for the task
    
    // For demonstration purposes, allocate memory for the stack
    task->stack_base = (uint32_t *)malloc(task->stack_size);
    if (task->stack_base == NULL) {
        return false;
    }
    
    // Clear the stack
    memset(task->stack_base, 0, task->stack_size);
    
    // Set up the initial stack pointer
    // In a real implementation, this would setup registers, PC, etc.
    task->stack_ptr = (uint32_t *)((uint8_t *)task->stack_base + task->stack_size - 4);
    
    // For demonstration, store the function pointer at the top of the stack
    task->stack_ptr[0] = (uint32_t)task->function;
    
    return true;
}