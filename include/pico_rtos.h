#ifndef PICO_RTOS_H
#define PICO_RTOS_H

#include "pico_rtos/mutex.h"
#include "pico_rtos/queue.h"
#include "pico_rtos/semaphore.h"
#include "pico_rtos/task.h"
#include "pico_rtos/timer.h"

/**
 * @brief Version information for Pico-RTOS
 */
#define PICO_RTOS_VERSION_MAJOR 0
#define PICO_RTOS_VERSION_MINOR 1
#define PICO_RTOS_VERSION_PATCH 0

/**
 * @brief Initialize the RTOS
 * 
 * This must be called before any other RTOS functions.
 * 
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_init(void);

/**
 * @brief Start the RTOS scheduler
 * 
 * This function does not return unless the scheduler is stopped.
 */
void pico_rtos_start(void);

/**
 * @brief Get the system tick count (milliseconds since startup)
 * 
 * @return Current system time in milliseconds
 */
uint32_t pico_rtos_get_tick_count(void);

/**
 * @brief Get the system uptime
 * 
 * @return System uptime in milliseconds
 */
uint32_t pico_rtos_get_uptime_ms(void);

/**
 * @brief Get the RTOS version string
 * 
 * @return Version string in format "X.Y.Z"
 */
const char *pico_rtos_get_version_string(void);

/**
 * @brief Enter a critical section (disable interrupts)
 */
void pico_rtos_enter_critical(void);

/**
 * @brief Exit a critical section (restore interrupts)
 */
void pico_rtos_exit_critical(void);

// Internal functions - not part of the public API but needed by other modules
void pico_rtos_scheduler(void);
void pico_rtos_scheduler_tick(void);
void pico_rtos_check_timers(void);
void pico_rtos_scheduler_add_task(pico_rtos_task_t *task);
void pico_rtos_schedule_next_task(void);
pico_rtos_timer_t *pico_rtos_get_first_timer(void);
pico_rtos_timer_t *pico_rtos_get_next_timer(pico_rtos_timer_t *timer);
void pico_rtos_add_timer(pico_rtos_timer_t *timer);
void pico_rtos_task_resume_highest_priority(void);

#endif // PICO_RTOS_H