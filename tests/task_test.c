#include <stdio.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

#define TASK_TEST_STACK_SIZE 256
#define TASK_TEST_PRIORITY_1 1
#define TASK_TEST_PRIORITY_2 2

void task_test_1(void *param) {
    while (1) {
        printf("Task 1: Running\n");
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

void task_test_2(void *param) {
    while (1) {
        printf("Task 2: Running\n");
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    pico_rtos_init();

    pico_rtos_task_t task_1;
    pico_rtos_task_create(&task_1, "Task Test 1", task_test_1, NULL, TASK_TEST_STACK_SIZE, TASK_TEST_PRIORITY_1);

    pico_rtos_task_t task_2;
    pico_rtos_task_create(&task_2, "Task Test 2", task_test_2, NULL, TASK_TEST_STACK_SIZE, TASK_TEST_PRIORITY_2);

    pico_rtos_start();

    return 0;
}