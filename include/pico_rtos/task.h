#ifndef PICO_RTOS_TASK_H
#define PICO_RTOS_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "types.h"



// Forward declaration for blocking objects
struct pico_rtos_block_object;

typedef struct pico_rtos_task {
    const char *name;
    pico_rtos_task_function_t function;
    void *param;
    uint32_t stack_size;
    uint32_t priority;
    uint32_t original_priority; // For priority inheritance
    pico_rtos_task_state_t state;
    uint32_t *stack_ptr;
    uint32_t *stack_base;
    uint32_t delay_until;
    bool auto_delete;
    pico_rtos_block_reason_t block_reason;
    struct pico_rtos_block_object *blocking_object;
    struct pico_rtos_task *next;  // For linked list of tasks
    pico_rtos_critical_section_t cs;
    
    // SMP-specific fields (v0.3.0)
#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    uint32_t core_affinity;                     // Core affinity setting (pico_rtos_core_affinity_t)
    uint32_t assigned_core;                     // Currently assigned core
    uint64_t cpu_time_us;                       // Total CPU time in microseconds
    uint32_t context_switch_count;              // Number of context switches for this task
    uint32_t stack_high_water_mark;             // Peak stack usage
    uint32_t migration_count;                   // Number of times task was migrated
    bool migration_pending;                     // Task is pending migration
    uint32_t last_run_core;                     // Last core this task ran on
    void *task_local_storage[4];                // Task-local storage slots
#endif
} pico_rtos_task_t;

/**
 * @brief Create a new task
 *
 * @param task Pointer to task structure
 * @param name Name of the task (for debugging)
 * @param function Task function to execute
 * @param param Parameter to pass to the task function
 * @param stack_size Size of the task's stack in bytes
 * @param priority Priority of the task (higher number = higher priority)
 * @return true if task creation was successful, false otherwise
 */
bool pico_rtos_task_create(pico_rtos_task_t *task, const char *name, 
                          pico_rtos_task_function_t function, void *param, 
                          uint32_t stack_size, uint32_t priority);

/**
 * @brief Suspend a task
 *
 * @param task Task to suspend, or NULL for current task
 */
void pico_rtos_task_suspend(pico_rtos_task_t *task);

/**
 * @brief Resume a suspended task
 *
 * @param task Task to resume
 */
void pico_rtos_task_resume(pico_rtos_task_t *task);

/**
 * @brief Delay the current task for the specified number of milliseconds
 *
 * @param ms Number of milliseconds to delay
 */
void pico_rtos_task_delay(uint32_t ms);

/**
 * @brief Get the current running task
 *
 * @return Pointer to the current task
 */
pico_rtos_task_t *pico_rtos_get_current_task(void);

/**
 * @brief Get task state as a string (for debugging)
 *
 * @param task Task to get state for
 * @return const char* String representation of the task state
 */
const char *pico_rtos_task_get_state_string(pico_rtos_task_t *task);

/**
 * @brief Delete a task and free its resources
 *
 * @param task Task to delete
 */
void pico_rtos_task_delete(pico_rtos_task_t *task);

/**
 * @brief Yield the current task (give up CPU voluntarily)
 * 
 * This allows other tasks of the same priority to run.
 */
void pico_rtos_task_yield(void);

/**
 * @brief Change a task's priority
 *
 * @param task Task to change priority for, or NULL for current task
 * @param new_priority New priority value
 * @return true if priority was changed successfully, false otherwise
 */
bool pico_rtos_task_set_priority(pico_rtos_task_t *task, uint32_t new_priority);

#endif // PICO_RTOS_TASK_H