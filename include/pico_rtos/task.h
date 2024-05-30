#ifndef PICO_RTOS_TASK_H
#define PICO_RTOS_TASK_H

#include <stdint.h>
#include "pico/critical_section.h"

#define PICO_RTOS_TASK_NAME_MAX_LENGTH 32

typedef void (*pico_rtos_task_function_t)(void *param);

typedef enum {
    PICO_RTOS_TASK_STATE_READY,
    PICO_RTOS_TASK_STATE_RUNNING,
    PICO_RTOS_TASK_STATE_SUSPENDED,
} pico_rtos_task_state_t;

typedef struct {
    const char *name;
    pico_rtos_task_function_t function;
    void *param;
    uint32_t stack_size;
    uint32_t priority;
    pico_rtos_task_state_t state;
    uint32_t *stack_ptr;
    critical_section_t cs;
} pico_rtos_task_t;

void pico_rtos_task_create(pico_rtos_task_t *task, const char *name, pico_rtos_task_function_t function, void *param, uint32_t stack_size, uint32_t priority);
void pico_rtos_task_suspend(pico_rtos_task_t *task);
void pico_rtos_task_resume(pico_rtos_task_t *task);
void pico_rtos_task_resume_highest_priority(void);
pico_rtos_task_t *pico_rtos_get_current_task(void);

#endif // PICO_RTOS_TASK_H