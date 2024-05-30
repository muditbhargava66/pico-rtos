#include "pico_rtos/task.h"
#include "pico/critical_section.h"

void pico_rtos_task_create(pico_rtos_task_t *task, const char *name, pico_rtos_task_function_t function, void *param, uint32_t stack_size, uint32_t priority) {
    task->name = name;
    task->function = function;
    task->param = param;
    task->stack_size = stack_size;
    task->priority = priority;
    task->state = PICO_RTOS_TASK_STATE_READY;
    task->stack_ptr = (uint32_t *)((uint8_t *)task + sizeof(pico_rtos_task_t) + stack_size - 4);
    task->stack_ptr[0] = (uint32_t)function;

    pico_rtos_scheduler_add_task(task);
}

void pico_rtos_task_suspend(pico_rtos_task_t *task) {
    critical_section_enter_blocking(&task->cs);
    task->state = PICO_RTOS_TASK_STATE_SUSPENDED;
    critical_section_exit(&task->cs);
}

void pico_rtos_task_resume(pico_rtos_task_t *task) {
    critical_section_enter_blocking(&task->cs);
    task->state = PICO_RTOS_TASK_STATE_READY;
    critical_section_exit(&task->cs);
}

void pico_rtos_task_resume_highest_priority(void) {
    pico_rtos_task_t *task = pico_rtos_scheduler_get_highest_priority_task();
    if (task != NULL) {
        pico_rtos_task_resume(task);
    }
}

pico_rtos_task_t *pico_rtos_get_current_task(void) {
    return pico_rtos_scheduler_get_current_task();
}