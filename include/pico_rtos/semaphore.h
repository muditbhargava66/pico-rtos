#ifndef PICO_RTOS_SEMAPHORE_H
#define PICO_RTOS_SEMAPHORE_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"

typedef struct {
    uint32_t count;
    uint32_t max_count;
    critical_section_t cs;
} pico_rtos_semaphore_t;

void pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count);
bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore);
bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout);

#endif // PICO_RTOS_SEMAPHORE_H