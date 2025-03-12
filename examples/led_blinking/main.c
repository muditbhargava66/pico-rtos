#include <stdio.h>
#include "pico/stdlib.h"
#include "../include/pico_rtos.h"

#define LED_PIN 25
#define BLINK_INTERVAL 500 // 500 ms

// LED ON task
void led_on_task(void *param) {
    while (1) {
        // Turn LED on
        gpio_put(LED_PIN, 1);
        printf("LED ON\n");
        
        // Wait for specified interval
        pico_rtos_task_delay(BLINK_INTERVAL);
    }
}

// LED OFF task
void led_off_task(void *param) {
    while (1) {
        // Turn LED off
        gpio_put(LED_PIN, 0);
        printf("LED OFF\n");
        
        // Wait for specified interval
        pico_rtos_task_delay(BLINK_INTERVAL);
    }
}

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize GPIO
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Initialize RTOS
    pico_rtos_init();
    
    // Create LED tasks
    pico_rtos_task_t led_on_task_handle;
    pico_rtos_task_create(&led_on_task_handle, "LED On Task", led_on_task, NULL, 256, 1);
    
    pico_rtos_task_t led_off_task_handle;
    pico_rtos_task_create(&led_off_task_handle, "LED Off Task", led_off_task, NULL, 256, 1);
    
    // Start the scheduler
    printf("Starting Pico-RTOS scheduler...\n");
    pico_rtos_start();
    
    // We should never reach here
    return 0;
}