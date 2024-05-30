#include <stdio.h>
#include "pico_rtos.h"

#define SEMAPHORE_TEST_TASK_STACK_SIZE 256
#define SEMAPHORE_TEST_TASK_PRIORITY 1
#define SEMAPHORE_MAX_COUNT 5

pico_rtos_semaphore_t semaphore;

void semaphore_test_task_1(void *param) {
    while (1) {
        pico_rtos_semaphore_take(&semaphore, PICO_RTOS_WAIT_FOREVER);
        printf("Task 1: Semaphore taken\n");
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

void semaphore_test_task_2(void *param) {
    while (1) {
        printf("Task 2: Giving semaphore\n");
        pico_rtos_semaphore_give(&semaphore);
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

int main() {
    pico_rtos_init();

    pico_rtos_semaphore_init(&semaphore, 0, SEMAPHORE_MAX_COUNT);

    pico_rtos_task_t task_1;
    pico_rtos_task_create(&task_1, "Semaphore Test Task 1", semaphore_test_task_1, NULL, SEMAPHORE_TEST_TASK_STACK_SIZE, SEMAPHORE_TEST_TASK_PRIORITY);

    pico_rtos_task_t task_2;
    pico_rtos_task_create(&task_2, "Semaphore Test Task 2", semaphore_test_task_2, NULL, SEMAPHORE_TEST_TASK_STACK_SIZE, SEMAPHORE_TEST_TASK_PRIORITY);

    pico_rtos_start();

    return 0;
}