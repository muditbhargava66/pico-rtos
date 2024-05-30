#ifndef PICO_RTOS_H
#define PICO_RTOS_H

#include "pico_rtos/mutex.h"
#include "pico_rtos/queue.h"
#include "pico_rtos/semaphore.h"
#include "pico_rtos/task.h"
#include "pico_rtos/timer.h"

void pico_rtos_init(void);
void pico_rtos_start(void);
uint32_t pico_rtos_get_system_time(void);
void pico_rtos_scheduler(void);
void pico_rtos_scheduler_add_task(pico_rtos_task_t *task);
pico_rtos_task_t *pico_rtos_scheduler_get_current_task(void);
pico_rtos_task_t *pico_rtos_scheduler_get_highest_priority_task(void);
pico_rtos_timer_t *pico_rtos_get_first_timer(void);
pico_rtos_timer_t *pico_rtos_get_next_timer(pico_rtos_timer_t *timer);

#endif // PICO_RTOS_H