#ifndef PICO_RTOS_TASK_H
#define PICO_RTOS_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"

#define PICO_RTOS_TASK_NAME_MAX_LENGTH 32
#define PICO_RTOS_WAIT_FOREVER UINT32_MAX
#define PICO_RTOS_NO_WAIT 0

typedef void (*pico_rtos_task_function_t)(void *param);

typedef enum {
    PICO_RTOS_TASK_STATE_READY,
    PICO_RTOS_TASK_STATE_RUNNING,
    PICO_RTOS_TASK_STATE_BLOCKED,    // Blocked on a resource or delay
    PICO_RTOS_TASK_STATE_SUSPENDED,  // Explicitly suspended
    PICO_RTOS_TASK_STATE_TERMINATED  // Task has terminated
} pico_rtos_task_state_t;

typedef enum {
    PICO_RTOS_BLOCK_REASON_NONE,
    PICO_RTOS_BLOCK_REASON_DELAY,
    PICO_RTOS_BLOCK_REASON_QUEUE_FULL,
    PICO_RTOS_BLOCK_REASON_QUEUE_EMPTY,
    PICO_RTOS_BLOCK_REASON_SEMAPHORE,
    PICO_RTOS_BLOCK_REASON_MUTEX
} pico_rtos_block_reason_t;

// Forward declaration for blocking objects
typedef struct pico_rtos_block_object pico_rtos_block_object_t;

typedef struct pico_rtos_task {
    const char *name;
    pico_rtos_task_function_t function;
    void *param;
    uint32_t stack_size;
    uint32_t priority;
    pico_rtos_task_state_t state;
    uint32_t *stack_ptr;
    uint32_t *stack_base;
    uint32_t delay_until;
    bool auto_delete;
    pico_rtos_block_reason_t block_reason;
    pico_rtos_block_object_t *blocking_object;
    struct pico_rtos_task *next;  // For linked list of tasks
    critical_section_t cs;
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

#endif // PICO_RTOS_TASK_H