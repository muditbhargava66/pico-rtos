#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico_rtos.h"
#include "pico_rtos/context_switch.h"

// Forward declarations (internal functions not exposed in header)
static void pico_rtos_tick_handler(void);
void pico_rtos_init_system_timer(void);

// Context switching functions (implemented in assembly)
extern void pico_rtos_context_switch(void);
extern void pico_rtos_start_first_task(void);
extern void PendSV_Handler(void);

// Global variables
static pico_rtos_task_t *current_task = NULL;
static pico_rtos_task_t *task_list = NULL;
static pico_rtos_timer_t *timer_list = NULL;
static uint32_t system_tick_count = 0;
static bool scheduler_running = false;
static critical_section_t scheduler_cs;

// Idle task variables
static pico_rtos_task_t idle_task;
static uint32_t idle_task_stack[64]; // 256 bytes stack for idle task
static uint32_t idle_task_counter = 0;

// Context switching variables (defined in context_switch.c)
extern uint32_t *current_task_stack_ptr;
extern uint32_t *next_task_stack_ptr;

// Define the block object for synchronization primitives
typedef struct pico_rtos_block_object {
    void *object;
    pico_rtos_task_t *blocked_tasks;
} pico_rtos_block_object_t;

// Version string
static const char *version_string = "0.2.0";

// Memory tracking variables
static uint32_t total_allocated_memory = 0;
static uint32_t peak_allocated_memory = 0;
static uint32_t allocation_count = 0;

// Interrupt nesting tracking
static uint32_t interrupt_nesting_level = 0;
static bool context_switch_pending = false;

// Helper function to safely compare times with overflow handling
static bool pico_rtos_time_after(uint32_t time1, uint32_t time2) {
    // Handle 32-bit overflow: time1 is after time2 if the difference is less than half the range
    return (int32_t)(time1 - time2) > 0;
}

// Helper function to safely check if a timeout has occurred
static bool pico_rtos_timeout_expired(uint32_t start_time, uint32_t current_time, uint32_t timeout) {
    if (timeout == PICO_RTOS_WAIT_FOREVER) {
        return false;
    }
    
    // Calculate elapsed time handling overflow
    uint32_t elapsed = current_time - start_time;
    return elapsed >= timeout;
}

// Idle task function - runs when no other tasks are ready
static void pico_rtos_idle_task_function(void *param) {
    (void)param; // Unused parameter
    
    while (1) {
        // Increment idle counter for statistics
        idle_task_counter++;
        
        // Check for stack overflow in all tasks periodically
        if (idle_task_counter % 1000 == 0) {
            pico_rtos_check_stack_overflow();
        }
        
        // Put CPU to sleep to save power
        __wfi(); // Wait for interrupt
    }
}

// Initialize the idle task
static bool pico_rtos_init_idle_task(void) {
    // Initialize idle task structure
    memset(&idle_task, 0, sizeof(pico_rtos_task_t));
    idle_task.name = "IDLE";
    idle_task.function = pico_rtos_idle_task_function;
    idle_task.param = NULL;
    idle_task.stack_size = sizeof(idle_task_stack);
    idle_task.priority = 0; // Lowest priority
    idle_task.state = PICO_RTOS_TASK_STATE_READY;
    idle_task.block_reason = PICO_RTOS_BLOCK_REASON_NONE;
    idle_task.auto_delete = false; // Static task, don't delete
    
    // Set up stack pointers
    idle_task.stack_base = idle_task_stack;
    
    // Set stack canary for idle task
    idle_task_stack[0] = 0xDEADBEEF;
    idle_task_stack[1] = 0xDEADBEEF;
    
    idle_task.stack_ptr = pico_rtos_init_task_stack(idle_task_stack, sizeof(idle_task_stack),
                                                   pico_rtos_idle_task_function, NULL);
    
    if (idle_task.stack_ptr == NULL) {
        return false;
    }
    
    // Initialize critical section
    critical_section_init(&idle_task.cs);
    
    // Add to task list manually (don't use scheduler_add_task to avoid malloc tracking)
    pico_rtos_enter_critical();
    if (task_list == NULL) {
        task_list = &idle_task;
    } else {
        // Add to the end of the list
        pico_rtos_task_t *last_task = task_list;
        while (last_task->next != NULL) {
            last_task = last_task->next;
        }
        last_task->next = &idle_task;
    }
    idle_task.next = NULL;
    pico_rtos_exit_critical();
    
    return true;
}

// Stack overflow detection
void pico_rtos_check_stack_overflow(void) {
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *task = task_list;
    while (task != NULL) {
        if (task->stack_base != NULL) {
            // Check for stack canary pattern (0xDEADBEEF) at the bottom of stack
            uint32_t *stack_bottom = task->stack_base;
            if (stack_bottom[0] != 0xDEADBEEF || stack_bottom[1] != 0xDEADBEEF) {
                // Stack overflow detected!
                pico_rtos_exit_critical();
                pico_rtos_handle_stack_overflow(task);
                return;
            }
        }
        task = task->next;
    }
    
    pico_rtos_exit_critical();
}

// Handle stack overflow - can be customized by user
__attribute__((weak)) void pico_rtos_handle_stack_overflow(pico_rtos_task_t *task) {
    // Default handler - print error and halt
    printf("STACK OVERFLOW DETECTED in task: %s\n", task->name ? task->name : "Unknown");
    printf("Task priority: %lu, Stack size: %lu\n", task->priority, task->stack_size);
    
    // Terminate the offending task
    task->state = PICO_RTOS_TASK_STATE_TERMINATED;
    
    // Trigger scheduler to switch to another task
    pico_rtos_context_switch();
    
    // If we get here, halt the system
    while (1) {
        __wfi();
    }
}

// Initialize the RTOS
bool pico_rtos_init(void) {
    // Initialize mutex for scheduler
    critical_section_init(&scheduler_cs);
    
    // Initialize context switching system
    pico_rtos_context_switch_init();
    
    // Initialize idle task
    if (!pico_rtos_init_idle_task()) {
        return false;
    }
    
    // Initialize system timer for tick generation
    pico_rtos_init_system_timer();
    
    return true;
}

// Start the RTOS scheduler
void pico_rtos_start(void) {
    if (!scheduler_running && task_list != NULL) {
        scheduler_running = true;
        
        // Find the highest priority task to start with
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
        
        if (highest_priority_task != NULL) {
            // Set current task and update stack pointer
            current_task = highest_priority_task;
            current_task->state = PICO_RTOS_TASK_STATE_RUNNING;
            current_task_stack_ptr = current_task->stack_ptr;
            
            // Start the first task using assembly function
            pico_rtos_start_first_task();
        }
        
        // We should never reach here unless scheduler is stopped
        scheduler_running = false;
    }
}

// Get the system tick count (thread-safe)
uint32_t pico_rtos_get_tick_count(void) {
    uint32_t tick_count;
    pico_rtos_enter_critical();
    tick_count = system_tick_count;
    pico_rtos_exit_critical();
    return tick_count;
}

// Get system uptime in milliseconds
uint32_t pico_rtos_get_uptime_ms(void) {
    return pico_rtos_get_tick_count(); // Use the thread-safe version
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

// Interrupt-safe context switch request
void pico_rtos_request_context_switch(void) {
    pico_rtos_enter_critical();
    
    // If we're in an interrupt, defer the context switch
    if (interrupt_nesting_level > 0) {
        context_switch_pending = true;
    } else {
        // Trigger immediate context switch
        pico_rtos_context_switch();
    }
    
    pico_rtos_exit_critical();
}

// Called when entering an interrupt
void pico_rtos_interrupt_enter(void) {
    interrupt_nesting_level++;
}

// Called when exiting an interrupt
void pico_rtos_interrupt_exit(void) {
    if (interrupt_nesting_level > 0) {
        interrupt_nesting_level--;
        
        // If this was the last nested interrupt and a context switch is pending
        if (interrupt_nesting_level == 0 && context_switch_pending) {
            context_switch_pending = false;
            pico_rtos_context_switch();
        }
    }
}

// Initialize the system timer for tick generation
void pico_rtos_init_system_timer(void) {
    // Configure a timer with a 1ms tick
    add_alarm_in_ms(1, (alarm_callback_t)pico_rtos_tick_handler, NULL, true);
}

// Handle system tick
static void pico_rtos_tick_handler(void) {
    pico_rtos_interrupt_enter();
    pico_rtos_enter_critical();
    
    // Increment tick counter
    system_tick_count++;
    
    // Check for any delayed tasks to unblock
    pico_rtos_task_t *task = task_list;
    while (task != NULL) {
        if (task->state == PICO_RTOS_TASK_STATE_BLOCKED && 
            task->block_reason == PICO_RTOS_BLOCK_REASON_DELAY &&
            pico_rtos_time_after(system_tick_count, task->delay_until)) {
            
            // Task delay has expired, move to ready state
            task->state = PICO_RTOS_TASK_STATE_READY;
            task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
        }
        task = task->next;
    }
    
    // Check for timer expiry
    pico_rtos_check_timers();
    
    // Clean up terminated tasks periodically (every 100ms)
    if (system_tick_count % 100 == 0) {
        pico_rtos_cleanup_terminated_tasks();
    }
    
    // Check if we need to switch tasks
    if (current_task->state != PICO_RTOS_TASK_STATE_RUNNING) {
        pico_rtos_schedule_next_task();
    }
    
    pico_rtos_exit_critical();
    pico_rtos_interrupt_exit();
    
    // Re-add the alarm for the next tick
    add_alarm_in_ms(1, (alarm_callback_t)pico_rtos_tick_handler, NULL, true);
}

// Schedule the next task to run
void pico_rtos_schedule_next_task(void) {
    // Find the highest priority task that is ready to run
    pico_rtos_task_t *highest_priority_task = pico_rtos_scheduler_get_highest_priority_task();
    
    // If no tasks are ready, run the idle task
    if (highest_priority_task == NULL) {
        highest_priority_task = &idle_task;
        idle_task.state = PICO_RTOS_TASK_STATE_READY;
    }
    
    // Switch to the highest priority task if it's different from current
    if (current_task != highest_priority_task) {
        pico_rtos_task_t *old_task = current_task;
        
        // Update task states
        if (old_task != NULL && old_task->state != PICO_RTOS_TASK_STATE_TERMINATED) {
            if (old_task->state == PICO_RTOS_TASK_STATE_RUNNING) {
                old_task->state = PICO_RTOS_TASK_STATE_READY;
            }
        }
        
        // Switch to new task
        current_task = highest_priority_task;
        current_task->state = PICO_RTOS_TASK_STATE_RUNNING;
        
        // Perform actual context switch
        pico_rtos_perform_context_switch(old_task, current_task);
    }
}

// Check and process expired timers
void pico_rtos_check_timers(void) {
    pico_rtos_timer_t *timer = timer_list;
    uint32_t current_time = system_tick_count;
    
    // List to store expired timers to execute callbacks outside critical section
    pico_rtos_timer_t *expired_timers[16]; // Support up to 16 expired timers per tick
    int expired_count = 0;
    
    while (timer != NULL && expired_count < 16) {
        // Handle timer overflow by checking if expiry time has passed
        bool timer_expired = false;
        if (timer->running && !timer->expired) {
            // Handle 32-bit overflow: if current_time wrapped around but expiry_time didn't,
            // or if both are in normal range and current >= expiry
            if ((current_time < timer->start_time && timer->expiry_time >= timer->start_time) ||
                (current_time >= timer->start_time && current_time >= timer->expiry_time)) {
                timer_expired = true;
            }
        }
        
        if (timer_expired) {
            // Timer has expired
            timer->expired = true;
            
            // Add to expired list for callback execution
            if (timer->callback != NULL) {
                expired_timers[expired_count++] = timer;
            }
            
            // Handle auto-reload
            if (timer->auto_reload) {
                timer->expired = false;
                timer->start_time = current_time;
                timer->expiry_time = current_time + timer->period;
            } else {
                timer->running = false;
            }
        }
        timer = timer->next;
    }
    
    // Execute timer callbacks outside critical section to prevent deadlocks
    if (expired_count > 0) {
        pico_rtos_exit_critical();
        
        for (int i = 0; i < expired_count; i++) {
            if (expired_timers[i]->callback != NULL) {
                expired_timers[i]->callback(expired_timers[i]->param);
            }
        }
        
        pico_rtos_enter_critical();
    }
}

// Get the current task (thread-safe)
pico_rtos_task_t *pico_rtos_get_current_task(void) {
    pico_rtos_task_t *task;
    pico_rtos_enter_critical();
    task = current_task;
    pico_rtos_exit_critical();
    return task;
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

// Remove a task from the scheduler and clean up its resources
void pico_rtos_scheduler_remove_task(pico_rtos_task_t *task) {
    if (task == NULL) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    // Find and remove the task from the list
    if (task_list == task) {
        // Removing the first task
        task_list = task->next;
    } else {
        // Find the task in the list
        pico_rtos_task_t *prev = task_list;
        while (prev != NULL && prev->next != task) {
            prev = prev->next;
        }
        
        if (prev != NULL) {
            prev->next = task->next;
        }
    }
    
    // Clean up task resources
    if (task->auto_delete && task->stack_base != NULL) {
        pico_rtos_free(task->stack_base, task->stack_size);
        task->stack_base = NULL;
        task->stack_ptr = NULL;
    }
    
    // Clear task fields
    task->next = NULL;
    task->state = PICO_RTOS_TASK_STATE_TERMINATED;
    
    pico_rtos_exit_critical();
}

// Clean up terminated tasks
void pico_rtos_cleanup_terminated_tasks(void) {
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *prev = NULL;
    pico_rtos_task_t *current = task_list;
    
    while (current != NULL) {
        pico_rtos_task_t *next = current->next;
        
        if (current->state == PICO_RTOS_TASK_STATE_TERMINATED) {
            // Remove from list
            if (prev == NULL) {
                task_list = next;
            } else {
                prev->next = next;
            }
            
            // Clean up resources
            if (current->auto_delete && current->stack_base != NULL) {
                pico_rtos_free(current->stack_base, current->stack_size);
                current->stack_base = NULL;
                current->stack_ptr = NULL;
            }
            
            // Don't advance prev since we removed current
        } else {
            prev = current;
        }
        
        current = next;
    }
    
    pico_rtos_exit_critical();
}

// Get the highest priority task (thread-safe)
pico_rtos_task_t *pico_rtos_scheduler_get_highest_priority_task(void) {
    pico_rtos_enter_critical();
    
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
    
    pico_rtos_exit_critical();
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

// Remove a timer from the timer list
void pico_rtos_remove_timer(pico_rtos_timer_t *timer) {
    if (timer == NULL) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    // Find and remove the timer from the list
    if (timer_list == timer) {
        // Removing the first timer
        timer_list = timer->next;
    } else {
        // Find the timer in the list
        pico_rtos_timer_t *prev = timer_list;
        while (prev != NULL && prev->next != timer) {
            prev = prev->next;
        }
        
        if (prev != NULL) {
            prev->next = timer->next;
        }
    }
    
    timer->next = NULL;
    
    pico_rtos_exit_critical();
}

// This function is no longer needed - task exit is handled in context_switch.c

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

// Function to resume the highest priority task from a blocking object
void pico_rtos_task_resume_highest_priority(void) {
    // This function is called when a synchronization primitive is released
    // It should trigger a context switch if a higher priority task becomes ready
    pico_rtos_task_t *highest_priority_task = pico_rtos_scheduler_get_highest_priority_task();
    if (highest_priority_task != NULL && 
        (current_task == NULL || highest_priority_task->priority > current_task->priority)) {
        // Trigger a context switch to the higher priority task
        pico_rtos_context_switch();
    }
}

// Memory tracking functions
void *pico_rtos_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr != NULL) {
        pico_rtos_enter_critical();
        total_allocated_memory += size;
        allocation_count++;
        if (total_allocated_memory > peak_allocated_memory) {
            peak_allocated_memory = total_allocated_memory;
        }
        pico_rtos_exit_critical();
    }
    return ptr;
}

void pico_rtos_free(void *ptr, size_t size) {
    if (ptr != NULL) {
        free(ptr);
        pico_rtos_enter_critical();
        if (total_allocated_memory >= size) {
            total_allocated_memory -= size;
        }
        pico_rtos_exit_critical();
    }
}

// Get memory statistics
void pico_rtos_get_memory_stats(uint32_t *current, uint32_t *peak, uint32_t *allocations) {
    pico_rtos_enter_critical();
    if (current) *current = total_allocated_memory;
    if (peak) *peak = peak_allocated_memory;
    if (allocations) *allocations = allocation_count;
    pico_rtos_exit_critical();
}

// Get idle task counter for CPU usage statistics
uint32_t pico_rtos_get_idle_counter(void) {
    uint32_t counter;
    pico_rtos_enter_critical();
    counter = idle_task_counter;
    pico_rtos_exit_critical();
    return counter;
}

// Get comprehensive system statistics
void pico_rtos_get_system_stats(pico_rtos_system_stats_t *stats) {
    if (stats == NULL) {
        return;
    }
    
    pico_rtos_enter_critical();
    
    // Initialize counters
    stats->total_tasks = 0;
    stats->ready_tasks = 0;
    stats->blocked_tasks = 0;
    stats->suspended_tasks = 0;
    stats->terminated_tasks = 0;
    
    // Count tasks by state
    pico_rtos_task_t *task = task_list;
    while (task != NULL) {
        stats->total_tasks++;
        switch (task->state) {
            case PICO_RTOS_TASK_STATE_READY:
                stats->ready_tasks++;
                break;
            case PICO_RTOS_TASK_STATE_RUNNING:
                stats->ready_tasks++; // Running task is essentially ready
                break;
            case PICO_RTOS_TASK_STATE_BLOCKED:
                stats->blocked_tasks++;
                break;
            case PICO_RTOS_TASK_STATE_SUSPENDED:
                stats->suspended_tasks++;
                break;
            case PICO_RTOS_TASK_STATE_TERMINATED:
                stats->terminated_tasks++;
                break;
        }
        task = task->next;
    }
    
    // Get memory statistics
    stats->current_memory = total_allocated_memory;
    stats->peak_memory = peak_allocated_memory;
    stats->total_allocations = allocation_count;
    
    // Get other system stats
    stats->idle_counter = idle_task_counter;
    stats->system_uptime = system_tick_count;
    
    pico_rtos_exit_critical();
}