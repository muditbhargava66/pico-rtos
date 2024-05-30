#ifndef PICO_RTOS_MUTEX_H
#define PICO_RTOS_MUTEX_H

#include "pico_rtos/task.h"
#include "pico/critical_section.h"

typedef struct {
    pico_rtos_task_t *owner;
    uint32_t lock_count;
    critical_section_t cs;
} pico_rtos_mutex_t;

void pico_rtos_mutex_init(pico_rtos_mutex_t *mutex);
void pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex);
void pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex);

#endif // PICO_RTOS_MUTEX_H