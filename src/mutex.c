#include "pico_rtos/mutex.h"
#include "pico/critical_section.h"

void pico_rtos_mutex_init(pico_rtos_mutex_t *mutex) {
    mutex->owner = NULL;
    mutex->lock_count = 0;
}

void pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex) {
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();

    critical_section_enter_blocking(&mutex->cs);

    if (mutex->owner == NULL) {
        mutex->owner = current_task;
        mutex->lock_count = 1;
    } else if (mutex->owner == current_task) {
        mutex->lock_count++;
    } else {
        pico_rtos_task_suspend(current_task);
        critical_section_exit(&mutex->cs);
        pico_rtos_scheduler();
        return;
    }

    critical_section_exit(&mutex->cs);
}

void pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex) {
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();

    critical_section_enter_blocking(&mutex->cs);

    if (mutex->owner == current_task) {
        mutex->lock_count--;
        if (mutex->lock_count == 0) {
            mutex->owner = NULL;
            pico_rtos_task_resume_highest_priority();
        }
    }

    critical_section_exit(&mutex->cs);
}