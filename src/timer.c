#include "pico_rtos/timer.h"
#include "pico/critical_section.h"

void pico_rtos_timer_init(pico_rtos_timer_t *timer, const char *name, pico_rtos_timer_callback_t callback, void *param, uint32_t period, bool auto_reload) {
    timer->name = name;
    timer->callback = callback;
    timer->param = param;
    timer->period = period;
    timer->auto_reload = auto_reload;
    timer->expired = false;
}

void pico_rtos_timer_start(pico_rtos_timer_t *timer) {
    critical_section_enter_blocking(&timer->cs);
    timer->expired = false;
    timer->start_time = pico_rtos_get_system_time();
    critical_section_exit(&timer->cs);
}

void pico_rtos_timer_stop(pico_rtos_timer_t *timer) {
    critical_section_enter_blocking(&timer->cs);
    timer->expired = true;
    critical_section_exit(&timer->cs);
}

void pico_rtos_timer_reset(pico_rtos_timer_t *timer) {
    critical_section_enter_blocking(&timer->cs);
    timer->start_time = pico_rtos_get_system_time();
    timer->expired = false;
    critical_section_exit(&timer->cs);
}

bool pico_rtos_timer_is_expired(pico_rtos_timer_t *timer) {
    critical_section_enter_blocking(&timer->cs);
    bool expired = timer->expired;
    critical_section_exit(&timer->cs);
    return expired;
}

void pico_rtos_timer_handler(void) {
    pico_rtos_timer_t *timer = pico_rtos_get_first_timer();
    while (timer != NULL) {
        uint32_t current_time = pico_rtos_get_system_time();
        if (!timer->expired && (current_time - timer->start_time) >= timer->period) {
            timer->callback(timer->param);
            if (timer->auto_reload) {
                timer->start_time = current_time;
            } else {
                timer->expired = true;
            }
        }
        timer = pico_rtos_get_next_timer(timer);
    }
}