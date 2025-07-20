#include "pico_rtos/timer.h"
#include "pico_rtos/error.h"
#include "pico/critical_section.h"

bool pico_rtos_timer_init(pico_rtos_timer_t *timer, const char *name, 
                         pico_rtos_timer_callback_t callback, void *param, 
                         uint32_t period, bool auto_reload) {
    if (timer == NULL) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_INVALID_POINTER, 0);
        return false;
    }
    
    if (period == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_TIMER_INVALID_PERIOD, period);
        return false;
    }
    
    timer->name = name;
    timer->callback = callback;
    timer->param = param;
    timer->period = period;
    timer->auto_reload = auto_reload;
    timer->running = false;
    timer->expired = false;
    timer->start_time = 0;
    timer->expiry_time = 0;
    critical_section_init(&timer->cs);
    timer->next = NULL;
    
    // Register timer with the RTOS
    extern void pico_rtos_add_timer(pico_rtos_timer_t *timer);
    pico_rtos_add_timer(timer);
    
    return true;
}

bool pico_rtos_timer_start(pico_rtos_timer_t *timer) {
    if (timer == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    
    timer->running = true;
    timer->expired = false;
    
    // Get current time and set expiry time
    extern uint32_t pico_rtos_get_tick_count(void);
    uint32_t current_time = pico_rtos_get_tick_count();
    timer->start_time = current_time;
    timer->expiry_time = current_time + timer->period;
    
    critical_section_exit(&timer->cs);
    return true;
}

bool pico_rtos_timer_stop(pico_rtos_timer_t *timer) {
    if (timer == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    
    bool was_running = timer->running;
    timer->running = false;
    
    critical_section_exit(&timer->cs);
    return was_running;
}

bool pico_rtos_timer_reset(pico_rtos_timer_t *timer) {
    if (timer == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    
    // Get current time and reset expiry time
    extern uint32_t pico_rtos_get_tick_count(void);
    uint32_t current_time = pico_rtos_get_tick_count();
    timer->start_time = current_time;
    timer->expiry_time = current_time + timer->period;
    timer->expired = false;
    
    critical_section_exit(&timer->cs);
    return true;
}

bool pico_rtos_timer_change_period(pico_rtos_timer_t *timer, uint32_t period) {
    if (timer == NULL || period == 0) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    
    timer->period = period;
    
    // If timer is running, update the expiry time
    if (timer->running) {
        extern uint32_t pico_rtos_get_tick_count(void);
        uint32_t current_time = pico_rtos_get_tick_count();
        timer->expiry_time = current_time + timer->period;
    }
    
    critical_section_exit(&timer->cs);
    return true;
}

bool pico_rtos_timer_is_running(pico_rtos_timer_t *timer) {
    if (timer == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    bool running = timer->running;
    critical_section_exit(&timer->cs);
    
    return running;
}

bool pico_rtos_timer_is_expired(pico_rtos_timer_t *timer) {
    if (timer == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    bool expired = timer->expired;
    critical_section_exit(&timer->cs);
    
    return expired;
}

uint32_t pico_rtos_timer_get_remaining_time(pico_rtos_timer_t *timer) {
    if (timer == NULL || !timer->running) {
        return 0;
    }
    
    critical_section_enter_blocking(&timer->cs);
    
    extern uint32_t pico_rtos_get_tick_count(void);
    uint32_t current_time = pico_rtos_get_tick_count();
    uint32_t remaining;
    
    // Handle overflow-safe time comparison
    uint32_t elapsed = current_time - timer->start_time;
    if (elapsed >= timer->period) {
        remaining = 0;
    } else {
        remaining = timer->period - elapsed;
    }
    
    critical_section_exit(&timer->cs);
    return remaining;
}

void pico_rtos_timer_delete(pico_rtos_timer_t *timer) {
    if (timer == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&timer->cs);
    
    timer->running = false;
    timer->expired = true;
    
    critical_section_exit(&timer->cs);
    
    // Remove timer from the timer list
    extern void pico_rtos_remove_timer(pico_rtos_timer_t *timer);
    pico_rtos_remove_timer(timer);
}

// Timer handler is now implemented in core.c to prevent deadlocks