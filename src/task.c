#include <stdlib.h>
#include <string.h>
#include "pico_rtos/task.h"
#include "pico_rtos/context_switch.h"
#include "pico_rtos/error.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include "pico/critical_section.h"

// Properly allocate and initialize task stack
static bool pico_rtos_setup_task_stack(pico_rtos_task_t *task);

bool pico_rtos_task_create(pico_rtos_task_t *task, const char *name, 
                          pico_rtos_task_function_t function, void *param, 
                          uint32_t stack_size, uint32_t priority) {
    if (task == NULL) {
        PICO_RTOS_LOG_TASK_ERROR("Task creation failed: NULL task pointer");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (function == NULL) {
        PICO_RTOS_LOG_TASK_ERROR("Task creation failed: NULL function pointer for task %s", 
                                name ? name : "unnamed");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_TASK_FUNCTION_NULL, 0);
        return false;
    }
    
    if (stack_size < 128) {
        PICO_RTOS_LOG_TASK_ERROR("Task creation failed: Stack size too small (%lu bytes) for task %s", 
                                stack_size, name ? name : "unnamed");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_TASK_STACK_TOO_SMALL, stack_size);
        return false; // Minimum stack size of 128 bytes
    }
    
    PICO_RTOS_LOG_TASK_INFO("Creating task: %s (priority %lu, stack %lu bytes)", 
                           name ? name : "unnamed", priority, stack_size);
    
    // Initialize task structure
    memset(task, 0, sizeof(pico_rtos_task_t));
    task->name = name;
    task->function = function;
    task->param = param;
    task->stack_size = stack_size;
    task->priority = priority;
    task->original_priority = priority;
    task->state = PICO_RTOS_TASK_STATE_READY;
    task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
    
    // Initialize critical section for this task
    critical_section_init(&task->cs);
    
    // Setup task stack
    if (!pico_rtos_setup_task_stack(task)) {
        PICO_RTOS_LOG_TASK_ERROR("Task creation failed: Stack setup failed for task %s", 
                                name ? name : "unnamed");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_OUT_OF_MEMORY, stack_size);
        return false;
    }
    
    // Add task to scheduler
    pico_rtos_scheduler_add_task(task);
    
    PICO_RTOS_LOG_TASK_DEBUG("Task %s created successfully", name ? name : "unnamed");
    return true;
}

void pico_rtos_task_suspend(pico_rtos_task_t *task) {
    pico_rtos_enter_critical();
    
    if (task == NULL) {
        // Suspend current task
        task = pico_rtos_get_current_task();
    }
    
    if (task != NULL) {
        PICO_RTOS_LOG_TASK_INFO("Suspending task: %s", task->name ? task->name : "unnamed");
        task->state = PICO_RTOS_TASK_STATE_SUSPENDED;
        
        // If suspending current task, trigger a context switch
        if (task == pico_rtos_get_current_task()) {
            pico_rtos_exit_critical();
            pico_rtos_scheduler(); // Trigger scheduler
            return;
        }
    } else {
        PICO_RTOS_LOG_TASK_WARN("Attempted to suspend NULL task");
    }
    
    pico_rtos_exit_critical();
}

void pico_rtos_task_resume(pico_rtos_task_t *task) {
    if (task == NULL) {
        PICO_RTOS_LOG_TASK_WARN("Attempted to resume NULL task");
        return;
    }
    
    pico_rtos_enter_critical();
    
    if (task->state == PICO_RTOS_TASK_STATE_SUSPENDED) {
        PICO_RTOS_LOG_TASK_INFO("Resuming task: %s", task->name ? task->name : "unnamed");
        task->state = PICO_RTOS_TASK_STATE_READY;
    } else {
        PICO_RTOS_LOG_TASK_DEBUG("Task %s was not suspended (state: %s)", 
                                task->name ? task->name : "unnamed",
                                pico_rtos_task_get_state_string(task));
    }
    
    pico_rtos_exit_critical();
}

void pico_rtos_task_delay(uint32_t ms) {
    if (ms == 0) {
        PICO_RTOS_LOG_TASK_DEBUG("Task delay called with 0ms, ignoring");
        return;
    }
    
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task != NULL) {
        PICO_RTOS_LOG_TASK_DEBUG("Task %s delaying for %lu ms", 
                                current_task->name ? current_task->name : "unnamed", ms);
        current_task->state = PICO_RTOS_TASK_STATE_BLOCKED;
        current_task->block_reason = PICO_RTOS_BLOCK_REASON_DELAY;
        current_task->delay_until = pico_rtos_get_tick_count() + ms;
        
        pico_rtos_exit_critical();
        pico_rtos_scheduler(); // Trigger scheduler to switch tasks
        return;
    } else {
        PICO_RTOS_LOG_TASK_WARN("Task delay called with no current task");
    }
    
    pico_rtos_exit_critical();
}

// External function implemented in core.c
extern pico_rtos_task_t *pico_rtos_get_current_task(void);

// Internal function to trigger the scheduler (defined in core.c)
void pico_rtos_scheduler(void) {
    // Trigger a context switch by calling the scheduler
    // This will use the PendSV interrupt for proper context switching
    pico_rtos_context_switch();
}

// Setup task stack with proper ARM Cortex-M0+ stack frame
static bool pico_rtos_setup_task_stack(pico_rtos_task_t *task) {
    if (task == NULL || task->function == NULL || task->stack_size == 0) {
        return false;
    }
    
    // Allocate memory for the stack using tracked allocation
    task->stack_base = (uint32_t *)pico_rtos_malloc(task->stack_size);
    if (task->stack_base == NULL) {
        return false;
    }
    
    // Clear the entire stack memory
    memset(task->stack_base, 0, task->stack_size);
    
    // Set stack canary for overflow detection at multiple locations
    task->stack_base[0] = 0xDEADBEEF;  // Bottom of stack
    task->stack_base[1] = 0xDEADBEEF;  // Second word for extra protection
    
    // Initialize the stack with proper ARM Cortex-M0+ stack frame
    task->stack_ptr = pico_rtos_init_task_stack(task->stack_base, task->stack_size, 
                                               task->function, task->param);
    
    if (task->stack_ptr == NULL) {
        // Stack initialization failed, clean up
        pico_rtos_free(task->stack_base, task->stack_size);
        task->stack_base = NULL;
        return false;
    }
    
    // Mark that this task's stack was dynamically allocated
    task->auto_delete = true;
    
    return true;
}

void pico_rtos_task_delete(pico_rtos_task_t *task) {
    if (task == NULL) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    // If deleting the current task, we need to schedule another task first
    if (task == pico_rtos_get_current_task()) {
        task->state = PICO_RTOS_TASK_STATE_TERMINATED;
        pico_rtos_exit_critical();
        
        // Trigger scheduler to switch to another task
        // The cleanup will happen in the next tick
        pico_rtos_scheduler();
        return;
    }
    
    // For other tasks, mark as terminated and remove immediately
    task->state = PICO_RTOS_TASK_STATE_TERMINATED;
    pico_rtos_exit_critical();
    
    // Remove from scheduler
    pico_rtos_scheduler_remove_task(task);
}

void pico_rtos_task_yield(void) {
    // Trigger a context switch to allow other tasks to run
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task != NULL && current_task->state == PICO_RTOS_TASK_STATE_RUNNING) {
        current_task->state = PICO_RTOS_TASK_STATE_READY;
    }
    
    pico_rtos_exit_critical();
    
    // Trigger scheduler to switch to another task
    pico_rtos_scheduler();
}

bool pico_rtos_task_set_priority(pico_rtos_task_t *task, uint32_t new_priority) {
    pico_rtos_enter_critical();
    
    if (task == NULL) {
        task = pico_rtos_get_current_task();
    }
    
    if (task == NULL) {
        pico_rtos_exit_critical();
        return false;
    }
    
    // Update both current and original priority
    task->priority = new_priority;
    task->original_priority = new_priority;
    
    pico_rtos_exit_critical();
    
    // Trigger scheduler in case priority change affects scheduling
    pico_rtos_scheduler();
    
    return true;
}