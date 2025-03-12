#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "pico_rtos.h"

// Forward declarations (internal functions not exposed in header)
static void pico_rtos_tick_handler(void);
// Note: pico_rtos_schedule_next_task is now non-static to match header declaration
void pico_rtos_init_system_timer(void);

// Global variables
static pico_rtos_task_t *current_task = NULL;
static pico_rtos_task_t *task_list = NULL;
static pico_rtos_timer_t *timer_list = NULL;
static uint32_t system_tick_count = 0;
static bool scheduler_running = false;
static critical_section_t scheduler_cs;

// Define the block object for synchronization primitives
typedef struct pico_rtos_block_object {
    void *object;
    pico_rtos_task_t *blocked_tasks;
} pico_rtos_block_object_t;

// Version string
static const char *version_string = "0.1.0";

// Initialize the RTOS
bool pico_rtos_init(void) {
    // Initialize mutex for scheduler
    critical_section_init(&scheduler_cs);
    
    // Initialize system timer for tick generation
    pico_rtos_init_system_timer();
    
    return true;
}

// Start the RTOS scheduler
void pico_rtos_start(void) {
    if (!scheduler_running && task_list != NULL) {
        scheduler_running = true;
        
        // Set current task to the highest priority task
        current_task = task_list;
        current_task->state = PICO_RTOS_TASK_STATE_RUNNING;
        
        // Start the first task
        pico_rtos_schedule_next_task();
        
        // We should never reach here unless scheduler is stopped
        scheduler_running = false;
    }
}

// Get the system tick count
uint32_t pico_rtos_get_tick_count(void) {
    return system_tick_count;
}

// Get system uptime in milliseconds
uint32_t pico_rtos_get_uptime_ms(void) {
    return system_tick_count;
}

// Get the RTOS version string
const char *pico_rtos_get_version_string(void) {
    return version_string;
}

// Enter a critical section
void pico_rtos_enter_critical(void) {
    critical_section_enter_blocking(&scheduler_cs);
}

// Exit a critical section
void pico_rtos_exit_critical(void) {
    critical_section_exit(&scheduler_cs);
}

// Initialize the system timer for tick generation
void pico_rtos_init_system_timer(void) {
    // Configure a timer with a 1ms tick
    add_alarm_in_ms(1, (alarm_callback_t)pico_rtos_tick_handler, NULL, true);
}

// Handle system tick
static void pico_rtos_tick_handler(void) {
    pico_rtos_enter_critical();
    
    // Increment tick counter
    system_tick_count++;
    
    // Check for any delayed tasks to unblock
    pico_rtos_task_t *task = task_list;
    while (task != NULL) {
        if (task->state == PICO_RTOS_TASK_STATE_BLOCKED && 
            task->block_reason == PICO_RTOS_BLOCK_REASON_DELAY &&
            task->delay_until <= system_tick_count) {
            
            // Task delay has expired, move to ready state
            task->state = PICO_RTOS_TASK_STATE_READY;
            task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
        }
        task = task->next;
    }
    
    // Check for timer expiry
    pico_rtos_check_timers();
    
    // Check if we need to switch tasks
    if (current_task->state != PICO_RTOS_TASK_STATE_RUNNING) {
        pico_rtos_schedule_next_task();
    }
    
    pico_rtos_exit_critical();
    
    // Re-add the alarm for the next tick
    add_alarm_in_ms(1, (alarm_callback_t)pico_rtos_tick_handler, NULL, true);
}

// Schedule the next task to run
void pico_rtos_schedule_next_task(void) {
    // Find the highest priority task that is ready to run
    pico_rtos_task_t *highest_priority_task = NULL;
    pico_rtos_task_t *task = task_list;
    
    while (task != NULL) {
        if (task->state == PICO_RTOS_TASK_STATE_READY) {
            if (highest_priority_task == NULL || 
                task->priority > highest_priority_task->priority) {
                highest_priority_task = task;
            }
        }
        task = task->next;
    }
    
    // If no tasks are ready, we might need to idle
    if (highest_priority_task == NULL) {
        // For now, just re-run the current task if it's not terminated
        if (current_task->state != PICO_RTOS_TASK_STATE_TERMINATED) {
            current_task->state = PICO_RTOS_TASK_STATE_RUNNING;
            return;
        }
        // Otherwise, we need to wait for an interrupt to make a task ready
        while (highest_priority_task == NULL) {
            // Enable interrupts and wait
            pico_rtos_exit_critical();
            __wfi(); // Wait for interrupt
            pico_rtos_enter_critical();
            
            // Check if any tasks became ready
            task = task_list;
            while (task != NULL) {
                if (task->state == PICO_RTOS_TASK_STATE_READY) {
                    if (highest_priority_task == NULL || 
                        task->priority > highest_priority_task->priority) {
                        highest_priority_task = task;
                    }
                }
                task = task->next;
            }
        }
    }
    
    // Switch to the highest priority task
    if (current_task != highest_priority_task) {
        pico_rtos_task_t *old_task = current_task;
        
        // Save context of current task if it's not terminated
        if (old_task->state != PICO_RTOS_TASK_STATE_TERMINATED) {
            if (old_task->state == PICO_RTOS_TASK_STATE_RUNNING) {
                old_task->state = PICO_RTOS_TASK_STATE_READY;
            }
            // Context save would happen here in a real implementation
        }
        
        // Switch to new task
        current_task = highest_priority_task;
        current_task->state = PICO_RTOS_TASK_STATE_RUNNING;
        
        // Context restore would happen here in a real implementation
    }
}

// Check and process expired timers
void pico_rtos_check_timers(void) {
    pico_rtos_timer_t *timer = timer_list;
    uint32_t current_time = system_tick_count;
    
    while (timer != NULL) {
        if (timer->running && !timer->expired && 
            (current_time >= timer->expiry_time)) {
            
            // Timer has expired
            timer->expired = true;
            
            // Execute callback if provided
            if (timer->callback != NULL) {
                // Execute outside critical section to allow other interrupts
                pico_rtos_exit_critical();
                timer->callback(timer->param);
                pico_rtos_enter_critical();
            }
            
            // Handle auto-reload
            if (timer->auto_reload) {
                timer->expired = false;
                timer->expiry_time = current_time + timer->period;
            } else {
                timer->running = false;
            }
        }
        timer = timer->next;
    }
}

// Get the current task
pico_rtos_task_t *pico_rtos_get_current_task(void) {
    return current_task;
}

// Add a task to the scheduler
void pico_rtos_scheduler_add_task(pico_rtos_task_t *task) {
    pico_rtos_enter_critical();
    
    // Add task to the task list
    if (task_list == NULL) {
        task_list = task;
    } else {
        // Add to the end of the list
        pico_rtos_task_t *last_task = task_list;
        while (last_task->next != NULL) {
            last_task = last_task->next;
        }
        last_task->next = task;
    }
    
    task->next = NULL;
    task->state = PICO_RTOS_TASK_STATE_READY;
    
    pico_rtos_exit_critical();
}

// Get the highest priority task
pico_rtos_task_t *pico_rtos_scheduler_get_highest_priority_task(void) {
    pico_rtos_task_t *highest_priority_task = NULL;
    pico_rtos_task_t *task = task_list;
    
    while (task != NULL) {
        if (task->state == PICO_RTOS_TASK_STATE_READY) {
            if (highest_priority_task == NULL || 
                task->priority > highest_priority_task->priority) {
                highest_priority_task = task;
            }
        }
        task = task->next;
    }
    
    return highest_priority_task;
}

// Add a timer to the timer list
void pico_rtos_add_timer(pico_rtos_timer_t *timer) {
    pico_rtos_enter_critical();
    
    // Add timer to the timer list
    if (timer_list == NULL) {
        timer_list = timer;
    } else {
        // Add to the end of the list
        pico_rtos_timer_t *last_timer = timer_list;
        while (last_timer->next != NULL) {
            last_timer = last_timer->next;
        }
        last_timer->next = timer;
    }
    
    timer->next = NULL;
    
    pico_rtos_exit_critical();
}

// Get the first timer in the list
pico_rtos_timer_t *pico_rtos_get_first_timer(void) {
    return timer_list;
}

// Get the next timer in the list
pico_rtos_timer_t *pico_rtos_get_next_timer(pico_rtos_timer_t *timer) {
    if (timer != NULL) {
        return timer->next;
    }
    return NULL;
}

// Trampoline function to start a task
static void pico_rtos_task_wrapper(void *param) {
    pico_rtos_task_t *task = (pico_rtos_task_t *)param;
    
    // Call the task function
    task->function(task->param);
    
    // Task has returned, mark it as terminated
    pico_rtos_enter_critical();
    task->state = PICO_RTOS_TASK_STATE_TERMINATED;
    pico_rtos_exit_critical();
    
    // Schedule the next task
    pico_rtos_schedule_next_task();
    
    // We should never reach here
    while (1) {
        // Spin
    }
}

// Get task state as a string
const char *pico_rtos_task_get_state_string(pico_rtos_task_t *task) {
    switch (task->state) {
        case PICO_RTOS_TASK_STATE_READY:
            return "Ready";
        case PICO_RTOS_TASK_STATE_RUNNING:
            return "Running";
        case PICO_RTOS_TASK_STATE_BLOCKED:
            return "Blocked";
        case PICO_RTOS_TASK_STATE_SUSPENDED:
            return "Suspended";
        case PICO_RTOS_TASK_STATE_TERMINATED:
            return "Terminated";
        default:
            return "Unknown";
    }
}

// Function to resume the highest priority task
void pico_rtos_task_resume_highest_priority(void) {
    pico_rtos_task_t *highest_priority_task = pico_rtos_scheduler_get_highest_priority_task();
    if (highest_priority_task != NULL) {
        highest_priority_task->state = PICO_RTOS_TASK_STATE_READY;
    }
}