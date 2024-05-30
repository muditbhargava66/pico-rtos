#include "pico_rtos/semaphore.h"
#include "pico/critical_section.h"

void pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count) {
    semaphore->count = initial_count;
    semaphore->max_count = max_count;
}

bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore) {
    critical_section_enter_blocking(&semaphore->cs);

    if (semaphore->count == semaphore->max_count) {
        critical_section_exit(&semaphore->cs);
        return false;
    }

    semaphore->count++;
    pico_rtos_task_resume_highest_priority();

    critical_section_exit(&semaphore->cs);
    return true;
}

bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout) {
    critical_section_enter_blocking(&semaphore->cs);

    if (semaphore->count == 0) {
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        pico_rtos_task_suspend(current_task);
        critical_section_exit(&semaphore->cs);
        pico_rtos_scheduler();
        return false;
    }

    semaphore->count--;

    critical_section_exit(&semaphore->cs);
    return true;
}