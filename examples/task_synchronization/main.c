#include <stdio.h>
#include "pico/stdlib.h"

// Disable migration warnings for examples (they already use v0.3.0 APIs)
#define PICO_RTOS_DISABLE_MIGRATION_WARNINGS
#include "pico_rtos.h"

#define BLINK_INTERVAL 500 // 500 ms

pico_rtos_semaphore_t semaphore;

void task_1(void *param) {
    while (1) {
        pico_rtos_semaphore_take(&semaphore, PICO_RTOS_WAIT_FOREVER);
        printf("Task 1: Semaphore taken\n");
        pico_rtos_task_delay(BLINK_INTERVAL);
        pico_rtos_semaphore_give(&semaphore);
        printf("Task 1: Semaphore given\n");
    }
}

void task_2(void *param) {
    while (1) {
        pico_rtos_semaphore_take(&semaphore, PICO_RTOS_WAIT_FOREVER);
        printf("Task 2: Semaphore taken\n");
        pico_rtos_task_delay(BLINK_INTERVAL);
        pico_rtos_semaphore_give(&semaphore);
        printf("Task 2: Semaphore given\n");
    }
}

int main() {
    stdio_init_all();

    pico_rtos_init();

    pico_rtos_semaphore_init(&semaphore, 1, 1);

    pico_rtos_task_t task_handle_1;
    pico_rtos_task_create(&task_handle_1, "Task 1", task_1, NULL, 256, 1);

    pico_rtos_task_t task_handle_2;
    pico_rtos_task_create(&task_handle_2, "Task 2", task_2, NULL, 256, 1);

    pico_rtos_start();

    return 0;
}