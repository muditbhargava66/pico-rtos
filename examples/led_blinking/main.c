#include <stdio.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

#define LED_PIN 25
#define BLINK_INTERVAL 1000 // 1000 ms (1 second)

// Shared LED state mutex
pico_rtos_mutex_t led_mutex;

// LED blinking task - properly controls LED state
void led_blink_task(void *param) {
    bool led_state = false;
    
    while (1) {
        // Acquire mutex to control LED
        if (pico_rtos_mutex_lock(&led_mutex, PICO_RTOS_WAIT_FOREVER)) {
            // Toggle LED state
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            printf("LED %s\n", led_state ? "ON" : "OFF");
            
            // Release mutex
            pico_rtos_mutex_unlock(&led_mutex);
        }
        
        // Wait for blink interval
        pico_rtos_task_delay(BLINK_INTERVAL);
    }
}

// Status reporting task - runs at higher priority
void status_task(void *param) {
    uint32_t counter = 0;
    
    while (1) {
        printf("System uptime: %lu ms, Status report #%lu\n", 
               pico_rtos_get_uptime_ms(), ++counter);
        
        // Report every 5 seconds
        pico_rtos_task_delay(5000);
    }
}

// Low priority background task
void background_task(void *param) {
    uint32_t work_counter = 0;
    
    while (1) {
        // Simulate background work
        work_counter++;
        
        // Print background work status every 10 seconds
        if (work_counter % 1000 == 0) {
            printf("Background task: completed %lu work units\n", work_counter);
        }
        
        // Small delay to prevent hogging CPU
        pico_rtos_task_delay(10);
    }
}

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize GPIO
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0); // Start with LED off
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize LED mutex
    if (!pico_rtos_mutex_init(&led_mutex)) {
        printf("Failed to initialize LED mutex\n");
        return -1;
    }
    
    printf("Starting Pico-RTOS LED Blinking Example...\n");
    
    // Create tasks with different priorities
    pico_rtos_task_t led_task_handle;
    if (!pico_rtos_task_create(&led_task_handle, "LED Blink", led_blink_task, NULL, 512, 2)) {
        printf("Failed to create LED task\n");
        return -1;
    }
    
    pico_rtos_task_t status_task_handle;
    if (!pico_rtos_task_create(&status_task_handle, "Status", status_task, NULL, 512, 3)) {
        printf("Failed to create status task\n");
        return -1;
    }
    
    pico_rtos_task_t bg_task_handle;
    if (!pico_rtos_task_create(&bg_task_handle, "Background", background_task, NULL, 256, 1)) {
        printf("Failed to create background task\n");
        return -1;
    }
    
    // Start the scheduler
    printf("Starting scheduler...\n");
    pico_rtos_start();
    
    // We should never reach here
    printf("ERROR: Scheduler returned unexpectedly\n");
    return -1;
}