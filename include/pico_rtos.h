#ifndef PICO_RTOS_H
#define PICO_RTOS_H

#include "pico_rtos/config.h"
#include "pico_rtos/error.h"
#include "pico_rtos/logging.h"
#include "pico_rtos/mutex.h"
#include "pico_rtos/queue.h"
#include "pico_rtos/semaphore.h"
#include "pico_rtos/task.h"
#include "pico_rtos/timer.h"
#include <stddef.h>

/**
 * @brief Version information for Pico-RTOS
 */
#define PICO_RTOS_VERSION_MAJOR 0
#define PICO_RTOS_VERSION_MINOR 2
#define PICO_RTOS_VERSION_PATCH 1

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
 * @brief Get the system tick rate in Hz
 * 
 * @return Current system tick frequency in Hz
 */
uint32_t pico_rtos_get_tick_rate_hz(void);

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
void pico_rtos_scheduler_remove_task(pico_rtos_task_t *task);
void pico_rtos_cleanup_terminated_tasks(void);
void pico_rtos_schedule_next_task(void);
pico_rtos_task_t *pico_rtos_scheduler_get_highest_priority_task(void);
pico_rtos_timer_t *pico_rtos_get_first_timer(void);
pico_rtos_timer_t *pico_rtos_get_next_timer(pico_rtos_timer_t *timer);
void pico_rtos_add_timer(pico_rtos_timer_t *timer);
void pico_rtos_remove_timer(pico_rtos_timer_t *timer);
void pico_rtos_task_resume_highest_priority(void);

// Memory management functions
void *pico_rtos_malloc(size_t size);
void pico_rtos_free(void *ptr, size_t size);
void pico_rtos_get_memory_stats(uint32_t *current, uint32_t *peak, uint32_t *allocations);

// Stack overflow protection
void pico_rtos_check_stack_overflow(void);
void pico_rtos_handle_stack_overflow(pico_rtos_task_t *task);

// System statistics
uint32_t pico_rtos_get_idle_counter(void);

// Idle task hook support
typedef void (*pico_rtos_idle_hook_t)(void);
void pico_rtos_set_idle_hook(pico_rtos_idle_hook_t hook);
void pico_rtos_clear_idle_hook(void);

// Interrupt handling
void pico_rtos_interrupt_enter(void);
void pico_rtos_interrupt_exit(void);
void pico_rtos_request_context_switch(void);

// System diagnostics
typedef struct {
    uint32_t total_tasks;
    uint32_t ready_tasks;
    uint32_t blocked_tasks;
    uint32_t suspended_tasks;
    uint32_t terminated_tasks;
    uint32_t current_memory;
    uint32_t peak_memory;
    uint32_t total_allocations;
    uint32_t idle_counter;
    uint32_t system_uptime;
} pico_rtos_system_stats_t;

void pico_rtos_get_system_stats(pico_rtos_system_stats_t *stats);

#endif // PICO_RTOS_H