#include <stdio.h>
#include "pico_rtos.h"

#define MUTEX_TEST_TASK_STACK_SIZE 256
#define MUTEX_TEST_TASK_PRIORITY 1

pico_rtos_mutex_t mutex;
int shared_resource = 0;

void mutex_test_task_1(void *param) {
    while (1) {
        pico_rtos_mutex_lock(&mutex);
        shared_resource++;
        printf("Task 1: Shared resource value: %d\n", shared_resource);
        pico_rtos_mutex_unlock(&mutex);
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

void mutex_test_task_2(void *param) {
    while (1) {
        pico_rtos_mutex_lock(&mutex);
        shared_resource++;
        printf("Task 2: Shared resource value: %d\n", shared_resource);
        pico_rtos_mutex_unlock(&mutex);
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

int main() {
    pico_rtos_init();

    pico_rtos_mutex_init(&mutex);

    pico_rtos_task_t task_1;
    pico_rtos_task_create(&task_1, "Mutex Test Task 1", mutex_test_task_1, NULL, MUTEX_TEST_TASK_STACK_SIZE, MUTEX_TEST_TASK_PRIORITY);

    pico_rtos_task_t task_2;
    pico_rtos_task_create(&task_2, "Mutex Test Task 2", mutex_test_task_2, NULL, MUTEX_TEST_TASK_STACK_SIZE, MUTEX_TEST_TASK_PRIORITY);

    pico_rtos_start();

    return 0;
}