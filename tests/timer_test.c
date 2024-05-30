#include <stdio.h>
#include "pico_rtos.h"

#define TIMER_TEST_PERIOD 1000 // 1 second

pico_rtos_timer_t timer;

void timer_callback(void *param) {
    printf("Timer callback: Timeout!\n");
}

int main() {
    pico_rtos_init();

    pico_rtos_timer_init(&timer, "Timer Test", timer_callback, NULL, TIMER_TEST_PERIOD, true);
    pico_rtos_timer_start(&timer);

    pico_rtos_start();

    return 0;
}