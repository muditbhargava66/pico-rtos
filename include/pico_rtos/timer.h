#ifndef PICO_RTOS_TIMER_H
#define PICO_RTOS_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"

#define PICO_RTOS_TIMER_NAME_MAX_LENGTH 32

typedef void (*pico_rtos_timer_callback_t)(void *param);

typedef struct pico_rtos_timer {
    const char *name;
    pico_rtos_timer_callback_t callback;
    void *param;
    uint32_t period;
    bool auto_reload;
    bool running;
    bool expired;
    uint32_t start_time;
    uint32_t expiry_time;
    critical_section_t cs;
    struct pico_rtos_timer *next;  // For linked list of timers
} pico_rtos_timer_t;

/**
 * @brief Initialize a timer
 * 
 * @param timer Pointer to timer structure
 * @param name Name of the timer (for debugging)
 * @param callback Function to call when timer expires
 * @param param Parameter to pass to the callback function
 * @param period Period of the timer in milliseconds
 * @param auto_reload Whether the timer should automatically reload after expiration
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_timer_init(pico_rtos_timer_t *timer, const char *name, 
                         pico_rtos_timer_callback_t callback, void *param, 
                         uint32_t period, bool auto_reload);

/**
 * @brief Start a timer
 * 
 * @param timer Pointer to timer structure
 * @return true if timer was started successfully, false otherwise
 */
bool pico_rtos_timer_start(pico_rtos_timer_t *timer);

/**
 * @brief Stop a timer
 * 
 * @param timer Pointer to timer structure
 * @return true if timer was stopped successfully, false otherwise
 */
bool pico_rtos_timer_stop(pico_rtos_timer_t *timer);

/**
 * @brief Reset a timer
 * 
 * @param timer Pointer to timer structure
 * @return true if timer was reset successfully, false otherwise
 */
bool pico_rtos_timer_reset(pico_rtos_timer_t *timer);

/**
 * @brief Change the period of a timer
 * 
 * @param timer Pointer to timer structure
 * @param period New period in milliseconds
 * @return true if period was changed successfully, false otherwise
 */
bool pico_rtos_timer_change_period(pico_rtos_timer_t *timer, uint32_t period);

/**
 * @brief Check if a timer is running
 * 
 * @param timer Pointer to timer structure
 * @return true if timer is running, false otherwise
 */
bool pico_rtos_timer_is_running(pico_rtos_timer_t *timer);

/**
 * @brief Check if a timer has expired
 * 
 * @param timer Pointer to timer structure
 * @return true if timer has expired, false otherwise
 */
bool pico_rtos_timer_is_expired(pico_rtos_timer_t *timer);

/**
 * @brief Get remaining time until timer expiration
 * 
 * @param timer Pointer to timer structure
 * @return Remaining time in milliseconds
 */
uint32_t pico_rtos_timer_get_remaining_time(pico_rtos_timer_t *timer);

/**
 * @brief Delete a timer and free associated resources
 * 
 * @param timer Pointer to timer structure
 */
void pico_rtos_timer_delete(pico_rtos_timer_t *timer);

#endif // PICO_RTOS_TIMER_H