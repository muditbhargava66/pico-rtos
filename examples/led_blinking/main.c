#include <stdio.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

#define LED_PIN 25
#define BLINK_INTERVAL 500 // 500 ms

void led_task_1(void *param) {
    while (1) {
        gpio_put(LED_PIN, 1);
        pico_rtos_task_suspend(pico_rtos_get_current_task());
        pico_rtos_task_delay(BLINK_INTERVAL);
    }
}

void led_task_2(void *param) {
    while (1) {
        gpio_put(LED_PIN, 0);
        pico_rtos_task_suspend(pico_rtos_get_current_task());
        pico_rtos_task_delay(BLINK_INTERVAL);
    }
}

int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    pico_rtos_init();

    pico_rtos_task_t task_1;
    pico_rtos_task_create(&task_1, "LED Task 1", led_task_1, NULL, 256, 1);

    pico_rtos_task_t task_2;
    pico_rtos_task_create(&task_2, "LED Task 2", led_task_2, NULL, 256, 1);

    pico_rtos_start();

    return 0;
}