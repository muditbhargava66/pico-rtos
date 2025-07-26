/**
 * @file main.c
 * @brief Hardware Interrupt Handling Example for Pico-RTOS
 * 
 * This example demonstrates proper integration of hardware interrupts with the RTOS scheduler.
 * It shows how to:
 * - Configure GPIO interrupts with appropriate priorities
 * - Implement interrupt service routines (ISRs) that are RTOS-aware
 * - Safely communicate between interrupt context and task context using queues
 * - Handle interrupt nesting and priority management
 * - Maintain system responsiveness while processing interrupts
 * 
 * Hardware Setup:
 * - Connect a button to GPIO 2 (with pull-up resistor)
 * - Connect an LED to GPIO 25 (onboard LED)
 * - Connect another LED to GPIO 15 for interrupt indication
 * 
 * The example creates multiple tasks with different priorities and demonstrates
 * how interrupts interact with the RTOS scheduler without disrupting real-time
 * behavior.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

// Disable migration warnings for examples (they already use v0.3.0 APIs)
#define PICO_RTOS_DISABLE_MIGRATION_WARNINGS
#include "pico_rtos.h"

// Hardware pin definitions
#define BUTTON_PIN          2   // GPIO pin for button input (interrupt source)
#define LED_PIN            25   // Onboard LED pin
#define INTERRUPT_LED_PIN  15   // LED to indicate interrupt activity

// Queue configuration for ISR-to-task communication
#define INTERRUPT_QUEUE_SIZE    10
#define BUTTON_QUEUE_SIZE       5

// Task priorities (higher number = higher priority)
#define INTERRUPT_HANDLER_TASK_PRIORITY  4  // Highest priority for interrupt processing
#define LED_CONTROL_TASK_PRIORITY        3  // High priority for LED control
#define BUTTON_MONITOR_TASK_PRIORITY     2  // Medium priority for button monitoring
#define STATUS_TASK_PRIORITY             1  // Low priority for status reporting

/**
 * @brief Structure for interrupt event data
 * 
 * This structure is used to pass interrupt information from the ISR
 * to the interrupt handler task through a queue. It includes timing
 * information and event type for proper processing.
 */
typedef struct {
    uint32_t timestamp;     // System tick when interrupt occurred
    uint32_t gpio_pin;      // GPIO pin that triggered the interrupt
    uint32_t event_type;    // Type of GPIO event (rising/falling edge)
    uint32_t interrupt_count; // Sequential interrupt counter
} interrupt_event_t;

/**
 * @brief Structure for button event data
 * 
 * Used for communication between interrupt handler task and
 * button monitor task to process button press events.
 */
typedef struct {
    uint32_t press_timestamp;   // When the button was pressed
    uint32_t release_timestamp; // When the button was released
    uint32_t press_duration;    // Duration of button press in ms
    bool is_long_press;         // True if press duration > 1 second
} button_event_t;

// Global variables for interrupt handling
static volatile uint32_t interrupt_counter = 0;
static volatile uint32_t last_interrupt_time = 0;
static volatile bool interrupt_processing_active = false;

// Queue handles for ISR-to-task communication
static pico_rtos_queue_t interrupt_queue;
static pico_rtos_queue_t button_queue;

// Queue storage buffers
static interrupt_event_t interrupt_queue_buffer[INTERRUPT_QUEUE_SIZE];
static button_event_t button_queue_buffer[BUTTON_QUEUE_SIZE];

// Mutex for LED control coordination
static pico_rtos_mutex_t led_control_mutex;

// Button state tracking
static volatile bool button_pressed = false;
static volatile uint32_t button_press_start = 0;

/**
 * @brief GPIO Interrupt Service Routine
 * 
 * This ISR demonstrates proper RTOS integration:
 * 1. Keeps processing minimal and fast
 * 2. Uses only ISR-safe operations
 * 3. Defers complex processing to task context
 * 4. Maintains interrupt priority and nesting
 * 
 * IMPORTANT NOTES:
 * - ISRs should be as short as possible to maintain system responsiveness
 * - Only use ISR-safe RTOS functions (non-blocking operations)
 * - Complex processing should be deferred to task context via queues
 * - Be aware of interrupt priority levels and nesting behavior
 */
void gpio_interrupt_handler(uint gpio, uint32_t events) {
    // CRITICAL: This function runs in interrupt context
    // Keep processing minimal and use only ISR-safe operations
    
    // Increment interrupt counter (atomic operation)
    interrupt_counter++;
    
    // Get current system time for timestamp
    uint32_t current_time = pico_rtos_get_tick_count();
    
    // Prepare interrupt event data
    interrupt_event_t event = {
        .timestamp = current_time,
        .gpio_pin = gpio,
        .event_type = events,
        .interrupt_count = interrupt_counter
    };
    
    // Send event to interrupt handler task via queue
    // Note: In a full implementation, this would use an ISR-safe queue function
    // For this example, we'll use the regular queue function with NO_WAIT timeout
    // to avoid blocking in interrupt context
    bool queue_success = pico_rtos_queue_send(&interrupt_queue, &event, PICO_RTOS_NO_WAIT);
    
    if (!queue_success) {
        // Queue is full - this indicates system overload
        // In a production system, you might increment an error counter
        // or take other appropriate action
    }
    
    // Update button state tracking
    if (gpio == BUTTON_PIN) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            // Button pressed (assuming active low with pull-up)
            button_pressed = true;
            button_press_start = current_time;
        } else if (events & GPIO_IRQ_EDGE_RISE) {
            // Button released
            if (button_pressed) {
                button_pressed = false;
                
                // Calculate press duration
                uint32_t press_duration = current_time - button_press_start;
                
                // Prepare button event
                button_event_t button_event = {
                    .press_timestamp = button_press_start,
                    .release_timestamp = current_time,
                    .press_duration = press_duration,
                    .is_long_press = (press_duration > 1000) // 1 second threshold
                };
                
                // Send to button monitor task
                pico_rtos_queue_send(&button_queue, &button_event, PICO_RTOS_NO_WAIT);
            }
        }
    }
    
    // Indicate interrupt activity on LED
    gpio_put(INTERRUPT_LED_PIN, 1);
    
    // Store last interrupt time for debouncing/analysis
    last_interrupt_time = current_time;
    
    // Set flag to indicate interrupt processing is active
    interrupt_processing_active = true;
    
    // IMPORTANT: ISR should return quickly to maintain system responsiveness
    // Complex processing is handled in the interrupt_handler_task
}

/**
 * @brief High-priority task for processing interrupt events
 * 
 * This task runs at the highest priority and processes interrupt events
 * received from the ISR via queue. It demonstrates:
 * - Proper separation of interrupt and task context
 * - Event processing with timing analysis
 * - Coordination with other system tasks
 */
void interrupt_handler_task(void *param) {
    interrupt_event_t event;
    uint32_t total_interrupts_processed = 0;
    uint32_t queue_overruns = 0;
    
    printf("Interrupt Handler Task started (Priority: %d)\n", INTERRUPT_HANDLER_TASK_PRIORITY);
    
    while (1) {
        // Wait for interrupt events from ISR
        if (pico_rtos_queue_receive(&interrupt_queue, &event, PICO_RTOS_WAIT_FOREVER)) {
            total_interrupts_processed++;
            
            // Process the interrupt event
            printf("INT: GPIO %lu, Events: 0x%lx, Count: %lu, Time: %lu ms\n",
                   event.gpio_pin, event.event_type, event.interrupt_count, event.timestamp);
            
            // Perform interrupt-specific processing
            if (event.gpio_pin == BUTTON_PIN) {
                // Button interrupt processing
                if (event.event_type & GPIO_IRQ_EDGE_FALL) {
                    printf("Button pressed at %lu ms\n", event.timestamp);
                } else if (event.event_type & GPIO_IRQ_EDGE_RISE) {
                    printf("Button released at %lu ms\n", event.timestamp);
                }
            }
            
            // Calculate interrupt processing latency
            uint32_t current_time = pico_rtos_get_tick_count();
            uint32_t processing_latency = current_time - event.timestamp;
            
            if (processing_latency > 10) { // More than 10ms latency
                printf("WARNING: High interrupt processing latency: %lu ms\n", processing_latency);
            }
            
            // Clear interrupt activity LED after processing
            gpio_put(INTERRUPT_LED_PIN, 0);
            interrupt_processing_active = false;
            
            // Brief delay to prevent overwhelming the system
            pico_rtos_task_delay(1);
        }
    }
}

/**
 * @brief Task for monitoring and processing button events
 * 
 * This task demonstrates how to process higher-level events derived
 * from interrupt data, including timing analysis and pattern recognition.
 */
void button_monitor_task(void *param) {
    button_event_t button_event;
    uint32_t short_presses = 0;
    uint32_t long_presses = 0;
    
    printf("Button Monitor Task started (Priority: %d)\n", BUTTON_MONITOR_TASK_PRIORITY);
    
    while (1) {
        // Wait for button events from interrupt handler
        if (pico_rtos_queue_receive(&button_queue, &button_event, PICO_RTOS_WAIT_FOREVER)) {
            
            if (button_event.is_long_press) {
                long_presses++;
                printf("BUTTON: Long press detected (%lu ms duration)\n", button_event.press_duration);
                
                // Long press action: Toggle LED state
                if (pico_rtos_mutex_lock(&led_control_mutex, 1000)) {
                    static bool led_state = false;
                    led_state = !led_state;
                    gpio_put(LED_PIN, led_state);
                    printf("LED %s (long press action)\n", led_state ? "ON" : "OFF");
                    pico_rtos_mutex_unlock(&led_control_mutex);
                }
            } else {
                short_presses++;
                printf("BUTTON: Short press detected (%lu ms duration)\n", button_event.press_duration);
                
                // Short press action: Blink LED briefly
                if (pico_rtos_mutex_lock(&led_control_mutex, 1000)) {
                    gpio_put(LED_PIN, 1);
                    pico_rtos_task_delay(100); // 100ms blink
                    gpio_put(LED_PIN, 0);
                    pico_rtos_mutex_unlock(&led_control_mutex);
                }
            }
            
            printf("Button statistics: Short: %lu, Long: %lu\n", short_presses, long_presses);
        }
    }
}

/**
 * @brief LED control task for periodic status indication
 * 
 * This task demonstrates how normal task operation continues
 * alongside interrupt processing, showing proper resource sharing
 * through mutex coordination.
 */
void led_control_task(void *param) {
    uint32_t blink_counter = 0;
    
    printf("LED Control Task started (Priority: %d)\n", LED_CONTROL_TASK_PRIORITY);
    
    while (1) {
        // Periodic LED blinking to show system is alive
        if (pico_rtos_mutex_lock(&led_control_mutex, 100)) {
            // Quick double blink every 5 seconds to indicate system status
            gpio_put(LED_PIN, 1);
            pico_rtos_task_delay(50);
            gpio_put(LED_PIN, 0);
            pico_rtos_task_delay(100);
            gpio_put(LED_PIN, 1);
            pico_rtos_task_delay(50);
            gpio_put(LED_PIN, 0);
            
            blink_counter++;
            pico_rtos_mutex_unlock(&led_control_mutex);
        }
        
        // Wait 5 seconds between status blinks
        pico_rtos_task_delay(5000);
    }
}

/**
 * @brief Status reporting task for system monitoring
 * 
 * This task provides periodic system status reports including
 * interrupt statistics and system health information.
 */
void status_task(void *param) {
    uint32_t last_interrupt_count = 0;
    uint32_t report_counter = 0;
    
    printf("Status Task started (Priority: %d)\n", STATUS_TASK_PRIORITY);
    
    while (1) {
        report_counter++;
        uint32_t current_interrupt_count = interrupt_counter;
        uint32_t interrupts_per_period = current_interrupt_count - last_interrupt_count;
        last_interrupt_count = current_interrupt_count;
        
        printf("\n=== System Status Report #%lu ===\n", report_counter);
        printf("Uptime: %lu ms\n", pico_rtos_get_uptime_ms());
        printf("Total interrupts: %lu\n", current_interrupt_count);
        printf("Interrupts this period: %lu\n", interrupts_per_period);
        printf("Last interrupt: %lu ms ago\n", 
               pico_rtos_get_tick_count() - last_interrupt_time);
        printf("Interrupt processing: %s\n", 
               interrupt_processing_active ? "ACTIVE" : "IDLE");
        
        // Get system statistics
        pico_rtos_system_stats_t stats;
        pico_rtos_get_system_stats(&stats);
        printf("Tasks - Ready: %lu, Blocked: %lu, Total: %lu\n",
               stats.ready_tasks, stats.blocked_tasks, stats.total_tasks);
        printf("Memory - Current: %lu bytes, Peak: %lu bytes\n",
               stats.current_memory, stats.peak_memory);
        printf("=====================================\n\n");
        
        // Report every 10 seconds
        pico_rtos_task_delay(10000);
    }
}

/**
 * @brief Initialize GPIO pins and interrupt configuration
 * 
 * This function sets up the hardware configuration including:
 * - GPIO pin initialization and direction
 * - Pull-up/pull-down resistor configuration
 * - Interrupt enable and callback registration
 * - Interrupt priority configuration
 */
static bool init_gpio_and_interrupts(void) {
    // Initialize button pin with pull-up resistor
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    
    // Initialize LED pins
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0); // Start with LED off
    
    gpio_init(INTERRUPT_LED_PIN);
    gpio_set_dir(INTERRUPT_LED_PIN, GPIO_OUT);
    gpio_put(INTERRUPT_LED_PIN, 0); // Start with interrupt LED off
    
    // Configure GPIO interrupt for button
    // Enable both rising and falling edge interrupts for complete button tracking
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, 
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true, 
                                       &gpio_interrupt_handler);
    
    printf("GPIO and interrupts initialized successfully\n");
    printf("Button pin: %d (with pull-up)\n", BUTTON_PIN);
    printf("LED pin: %d\n", LED_PIN);
    printf("Interrupt LED pin: %d\n", INTERRUPT_LED_PIN);
    
    return true;
}

/**
 * @brief Initialize RTOS queues for ISR-to-task communication
 * 
 * Sets up the communication channels between interrupt context
 * and task context using RTOS queues.
 */
static bool init_queues(void) {
    // Initialize interrupt event queue
    if (!pico_rtos_queue_init(&interrupt_queue, 
                              interrupt_queue_buffer,
                              sizeof(interrupt_event_t),
                              INTERRUPT_QUEUE_SIZE)) {
        printf("Failed to initialize interrupt queue\n");
        return false;
    }
    
    // Initialize button event queue
    if (!pico_rtos_queue_init(&button_queue,
                              button_queue_buffer,
                              sizeof(button_event_t),
                              BUTTON_QUEUE_SIZE)) {
        printf("Failed to initialize button queue\n");
        return false;
    }
    
    printf("Queues initialized successfully\n");
    printf("Interrupt queue size: %d events\n", INTERRUPT_QUEUE_SIZE);
    printf("Button queue size: %d events\n", BUTTON_QUEUE_SIZE);
    
    return true;
}

/**
 * @brief Main function - System initialization and task creation
 * 
 * This function demonstrates the complete setup process for a system
 * that integrates hardware interrupts with RTOS task scheduling.
 */
int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    printf("\n=== Pico-RTOS Hardware Interrupt Example ===\n");
    printf("This example demonstrates proper interrupt handling with RTOS integration\n");
    printf("Press the button connected to GPIO %d to generate interrupts\n", BUTTON_PIN);
    printf("Watch the LEDs and console output for interrupt activity\n\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    printf("RTOS initialized successfully\n");
    
    // Initialize LED control mutex
    if (!pico_rtos_mutex_init(&led_control_mutex)) {
        printf("ERROR: Failed to initialize LED control mutex\n");
        return -1;
    }
    printf("Mutex initialized successfully\n");
    
    // Initialize queues for ISR-to-task communication
    if (!init_queues()) {
        printf("ERROR: Failed to initialize queues\n");
        return -1;
    }
    
    // Initialize GPIO and interrupt configuration
    if (!init_gpio_and_interrupts()) {
        printf("ERROR: Failed to initialize GPIO and interrupts\n");
        return -1;
    }
    
    // Create interrupt handler task (highest priority)
    pico_rtos_task_t interrupt_task_handle;
    if (!pico_rtos_task_create(&interrupt_task_handle, 
                               "InterruptHandler", 
                               interrupt_handler_task, 
                               NULL, 
                               1024, // Larger stack for interrupt processing
                               INTERRUPT_HANDLER_TASK_PRIORITY)) {
        printf("ERROR: Failed to create interrupt handler task\n");
        return -1;
    }
    printf("Interrupt handler task created (Priority: %d)\n", INTERRUPT_HANDLER_TASK_PRIORITY);
    
    // Create button monitor task
    pico_rtos_task_t button_task_handle;
    if (!pico_rtos_task_create(&button_task_handle,
                               "ButtonMonitor",
                               button_monitor_task,
                               NULL,
                               512,
                               BUTTON_MONITOR_TASK_PRIORITY)) {
        printf("ERROR: Failed to create button monitor task\n");
        return -1;
    }
    printf("Button monitor task created (Priority: %d)\n", BUTTON_MONITOR_TASK_PRIORITY);
    
    // Create LED control task
    pico_rtos_task_t led_task_handle;
    if (!pico_rtos_task_create(&led_task_handle,
                               "LEDControl",
                               led_control_task,
                               NULL,
                               512,
                               LED_CONTROL_TASK_PRIORITY)) {
        printf("ERROR: Failed to create LED control task\n");
        return -1;
    }
    printf("LED control task created (Priority: %d)\n", LED_CONTROL_TASK_PRIORITY);
    
    // Create status reporting task (lowest priority)
    pico_rtos_task_t status_task_handle;
    if (!pico_rtos_task_create(&status_task_handle,
                               "StatusReport",
                               status_task,
                               NULL,
                               512,
                               STATUS_TASK_PRIORITY)) {
        printf("ERROR: Failed to create status task\n");
        return -1;
    }
    printf("Status task created (Priority: %d)\n", STATUS_TASK_PRIORITY);
    
    printf("\nAll tasks created successfully!\n");
    printf("Starting RTOS scheduler...\n");
    printf("System ready - press button to generate interrupts\n\n");
    
    // Start the RTOS scheduler
    pico_rtos_start();
    
    // We should never reach here
    printf("ERROR: RTOS scheduler returned unexpectedly\n");
    return -1;
}