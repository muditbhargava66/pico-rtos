#include <stdio.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

#define QUEUE_TEST_TASK_STACK_SIZE 256
#define QUEUE_TEST_TASK_PRIORITY 1
#define QUEUE_SIZE 5

pico_rtos_queue_t queue;
int queue_buffer[QUEUE_SIZE];

void queue_test_task_1(void *param) {
    int item = 0;
    while (1) {
        item++;
        printf("Task 1: Sending item %d to the queue\n", item);
        pico_rtos_queue_send(&queue, &item, 0);
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

void queue_test_task_2(void *param) {
    int item;
    while (1) {
        pico_rtos_queue_receive(&queue, &item, PICO_RTOS_WAIT_FOREVER);
        printf("Task 2: Received item %d from the queue\n", item);
        pico_rtos_task_suspend(pico_rtos_get_current_task());
    }
}

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    pico_rtos_init();

    pico_rtos_queue_init(&queue, queue_buffer, sizeof(int), QUEUE_SIZE);

    pico_rtos_task_t task_1;
    pico_rtos_task_create(&task_1, "Queue Test Task 1", queue_test_task_1, NULL, QUEUE_TEST_TASK_STACK_SIZE, QUEUE_TEST_TASK_PRIORITY);

    pico_rtos_task_t task_2;
    pico_rtos_task_create(&task_2, "Queue Test Task 2", queue_test_task_2, NULL, QUEUE_TEST_TASK_STACK_SIZE, QUEUE_TEST_TASK_PRIORITY);

    pico_rtos_start();

    return 0;
}