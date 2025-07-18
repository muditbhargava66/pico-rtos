#include <stdio.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

#define LED_PIN 25

// Test mutexes and priority inheritance
pico_rtos_mutex_t test_mutex;
pico_rtos_semaphore_t test_semaphore;
pico_rtos_queue_t test_queue;
uint8_t queue_buffer[5 * sizeof(int)];

// High priority task that tests priority inheritance
void high_priority_task(void *param) {
    printf("High priority task started\n");
    
    while (1) {
        printf("High priority task: Attempting to acquire mutex\n");
        
        if (pico_rtos_mutex_lock(&test_mutex, 1000)) {
            printf("High priority task: Got mutex, working...\n");
            pico_rtos_task_delay(500);
            pico_rtos_mutex_unlock(&test_mutex);
            printf("High priority task: Released mutex\n");
        } else {
            printf("High priority task: Mutex timeout\n");
        }
        
        pico_rtos_task_delay(2000);
    }
}

// Medium priority task that holds mutex for a long time
void medium_priority_task(void *param) {
    printf("Medium priority task started\n");
    
    while (1) {
        printf("Medium priority task: Acquiring mutex\n");
        
        if (pico_rtos_mutex_lock(&test_mutex, PICO_RTOS_WAIT_FOREVER)) {
            printf("Medium priority task: Got mutex, holding for 2 seconds...\n");
            pico_rtos_task_delay(2000); // Hold mutex for 2 seconds
            pico_rtos_mutex_unlock(&test_mutex);
            printf("Medium priority task: Released mutex\n");
        }
        
        pico_rtos_task_delay(3000);
    }
}

// Low priority task that does background work
void low_priority_task(void *param) {
    uint32_t counter = 0;
    
    while (1) {
        counter++;
        
        if (counter % 500 == 0) {
            printf("Low priority task: Background work counter = %lu\n", counter);
        }
        
        pico_rtos_task_delay(10);
    }
}

// System monitor task
void monitor_task(void *param) {
    pico_rtos_system_stats_t stats;
    
    while (1) {
        pico_rtos_get_system_stats(&stats);
        
        printf("\n=== SYSTEM STATISTICS ===\n");
        printf("Total tasks: %lu\n", stats.total_tasks);
        printf("Ready tasks: %lu\n", stats.ready_tasks);
        printf("Blocked tasks: %lu\n", stats.blocked_tasks);
        printf("Suspended tasks: %lu\n", stats.suspended_tasks);
        printf("Terminated tasks: %lu\n", stats.terminated_tasks);
        printf("Current memory: %lu bytes\n", stats.current_memory);
        printf("Peak memory: %lu bytes\n", stats.peak_memory);
        printf("Total allocations: %lu\n", stats.total_allocations);
        printf("Idle counter: %lu\n", stats.idle_counter);
        printf("System uptime: %lu ms\n", stats.system_uptime);
        printf("========================\n\n");
        
        pico_rtos_task_delay(10000); // Report every 10 seconds
    }
}

// Queue test task - sender
void queue_sender_task(void *param) {
    int data = 0;
    
    while (1) {
        data++;
        
        if (pico_rtos_queue_send(&test_queue, &data, 1000)) {
            printf("Queue sender: Sent %d\n", data);
        } else {
            printf("Queue sender: Send timeout\n");
        }
        
        pico_rtos_task_delay(1500);
    }
}

// Queue test task - receiver
void queue_receiver_task(void *param) {
    int received_data;
    
    while (1) {
        if (pico_rtos_queue_receive(&test_queue, &received_data, 2000)) {
            printf("Queue receiver: Received %d\n", received_data);
        } else {
            printf("Queue receiver: Receive timeout\n");
        }
        
        pico_rtos_task_delay(1000);
    }
}

// LED blink task
void led_task(void *param) {
    bool led_state = false;
    
    while (1) {
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        printf("LED: %s\n", led_state ? "ON" : "OFF");
        
        pico_rtos_task_delay(2000);
    }
}

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize GPIO
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    printf("Starting Pico-RTOS System Test...\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize synchronization objects
    if (!pico_rtos_mutex_init(&test_mutex)) {
        printf("Failed to initialize mutex\n");
        return -1;
    }
    
    if (!pico_rtos_semaphore_init(&test_semaphore, 1, 1)) {
        printf("Failed to initialize semaphore\n");
        return -1;
    }
    
    if (!pico_rtos_queue_init(&test_queue, queue_buffer, sizeof(int), 5)) {
        printf("Failed to initialize queue\n");
        return -1;
    }
    
    printf("Creating tasks...\n");
    
    // Create tasks with different priorities to test scheduling and priority inheritance
    pico_rtos_task_t high_task, medium_task, low_task, monitor_task_handle;
    pico_rtos_task_t queue_sender_handle, queue_receiver_handle, led_task_handle;
    
    // High priority task (priority 5)
    if (!pico_rtos_task_create(&high_task, "High Priority", high_priority_task, NULL, 512, 5)) {
        printf("Failed to create high priority task\n");
        return -1;
    }
    
    // Medium priority task (priority 3)
    if (!pico_rtos_task_create(&medium_task, "Medium Priority", medium_priority_task, NULL, 512, 3)) {
        printf("Failed to create medium priority task\n");
        return -1;
    }
    
    // Low priority task (priority 1)
    if (!pico_rtos_task_create(&low_task, "Low Priority", low_priority_task, NULL, 256, 1)) {
        printf("Failed to create low priority task\n");
        return -1;
    }
    
    // System monitor task (priority 4)
    if (!pico_rtos_task_create(&monitor_task_handle, "Monitor", monitor_task, NULL, 1024, 4)) {
        printf("Failed to create monitor task\n");
        return -1;
    }
    
    // Queue test tasks (priority 2)
    if (!pico_rtos_task_create(&queue_sender_handle, "Queue Sender", queue_sender_task, NULL, 256, 2)) {
        printf("Failed to create queue sender task\n");
        return -1;
    }
    
    if (!pico_rtos_task_create(&queue_receiver_handle, "Queue Receiver", queue_receiver_task, NULL, 256, 2)) {
        printf("Failed to create queue receiver task\n");
        return -1;
    }
    
    // LED task (priority 1)
    if (!pico_rtos_task_create(&led_task_handle, "LED", led_task, NULL, 256, 1)) {
        printf("Failed to create LED task\n");
        return -1;
    }
    
    printf("All tasks created successfully!\n");
    printf("Starting scheduler...\n");
    
    // Start the scheduler
    pico_rtos_start();
    
    // We should never reach here
    printf("ERROR: Scheduler returned unexpectedly\n");
    return -1;
}