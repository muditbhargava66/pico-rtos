#ifndef PICO_RTOS_TIMER_H
#define PICO_RTOS_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"

#define PICO_RTOS_TIMER_NAME_MAX_LENGTH 32

typedef void (*pico_rtos_timer_callback_t)(void *param);

typedef struct {
    const char *name;
    pico_rtos_timer_callback_t callback;
    void *param;
    uint32_t period;
    bool auto_reload;
    bool expired;
    uint32_t start_time;
    critical_section_t cs;
} pico_rtos_timer_t;

void pico_rtos_timer_init(pico_rtos_timer_t *timer, const char *name, pico_rtos_timer_callback_t callback, void *param, uint32_t period, bool auto_reload);
void pico_rtos_timer_start(pico_rtos_timer_t *timer);
void pico_rtos_timer_stop(pico_rtos_timer_t *timer);
void pico_rtos_timer_reset(pico_rtos_timer_t *timer);
bool pico_rtos_timer_is_expired(pico_rtos_timer_t *timer);
void pico_rtos_timer_handler(void);

#endif // PICO_RTOS_TIMER_H