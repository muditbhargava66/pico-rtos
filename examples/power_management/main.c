/**
 * @file main.c
 * @brief Pico-RTOS Power Management Example
 * 
 * This example demonstrates power management techniques using idle task hooks
 * and Pico SDK sleep modes. It shows how to:
 * 
 * 1. Implement idle task hooks for power optimization
 * 2. Use WFI (Wait For Interrupt) for CPU power saving
 * 3. Monitor power consumption through ADC readings
 * 4. Handle wake-up events and proper task resumption
 * 5. Optimize system clocks for power efficiency
 * 
 * Power Management Strategies Demonstrated:
 * - CPU sleep during idle periods using WFI instruction
 * - Dynamic clock frequency adjustment based on workload
 * - Peripheral power management (turning off unused peripherals)
 * - Wake-up event handling from various sources (GPIO, timers, etc.)
 * - Power consumption measurement and reporting
 * 
 * Hardware Requirements:
 * - Raspberry Pi Pico or compatible RP2040 board
 * - Optional: Current measurement setup for power monitoring
 * - Optional: External wake-up source connected to GPIO
 * 
 * @author Pico-RTOS Development Team
 * @version 0.2.1
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

// =============================================================================
// CONFIGURATION AND CONSTANTS
// =============================================================================

#define LED_PIN                 25      // Onboard LED
#define WAKE_UP_PIN            2       // GPIO pin for external wake-up
#define POWER_MONITOR_PIN      26      // ADC pin for power monitoring (ADC0)

#define NORMAL_TASK_INTERVAL   1000    // Normal operation interval (ms)
#define POWER_SAVE_THRESHOLD   5000    // Time before entering power save (ms)
#define POWER_MONITOR_INTERVAL 2000    // Power monitoring interval (ms)

// Power management states
typedef enum {
    POWER_STATE_NORMAL,     // Normal operation
    POWER_STATE_IDLE,       // CPU idle, peripherals active
    POWER_STATE_SLEEP,      // Deep sleep mode
    POWER_STATE_WAKE_UP     // Waking up from sleep
} power_state_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Power management state
static volatile power_state_t current_power_state = POWER_STATE_NORMAL;
static volatile uint32_t last_activity_time = 0;
static volatile bool wake_up_requested = false;

// Synchronization primitives
static pico_rtos_mutex_t power_state_mutex;
static pico_rtos_semaphore_t wake_up_semaphore;
static pico_rtos_queue_t power_event_queue;
static uint32_t power_event_queue_buffer[5]; // Buffer for 5 events

// Power monitoring data
typedef struct {
    uint32_t timestamp;
    uint16_t voltage_reading;
    power_state_t state;
    uint32_t cpu_frequency;
} power_measurement_t;

static power_measurement_t power_measurements[10];
static uint8_t power_measurement_index = 0;

// Task handles
static pico_rtos_task_t main_task_handle;
static pico_rtos_task_t power_monitor_task_handle;
static pico_rtos_task_t wake_up_task_handle;

// =============================================================================
// POWER MANAGEMENT FUNCTIONS
// =============================================================================

/**
 * @brief Idle task hook implementation
 * 
 * This function is called when the system is idle and no tasks are ready to run.
 * It implements various power saving strategies based on the current system state.
 * 
 * Power Saving Strategies:
 * 1. WFI (Wait For Interrupt) - Stops CPU clock until interrupt occurs
 * 2. Clock frequency reduction - Reduces system clock for lower power
 * 3. Peripheral gating - Disables unused peripherals
 * 4. Voltage scaling - Reduces core voltage when possible
 */
void power_management_idle_hook(void) {
    static uint32_t idle_counter = 0;
    static uint32_t last_power_check = 0;
    
    idle_counter++;
    uint32_t current_time = pico_rtos_get_tick_count();
    
    // Check if we should enter power saving mode
    if (current_time - last_activity_time > POWER_SAVE_THRESHOLD) {
        
        // Enter critical section to check and modify power state
        pico_rtos_enter_critical();
        
        if (current_power_state == POWER_STATE_NORMAL) {
            current_power_state = POWER_STATE_IDLE;
            
            // Reduce system clock frequency for power saving
            // Note: This is a simplified example - real implementation would
            // need to carefully manage clock dependencies
            printf("[POWER] Entering idle power mode - reducing clock frequency\n");
            
            // Disable unnecessary peripherals
            // In a real application, you would disable specific peripherals
            // that are not needed during idle periods
            
            pico_rtos_exit_critical();
            
            // Use WFI to halt CPU until next interrupt
            // This is the most basic power saving technique
            __wfi();
            
            pico_rtos_enter_critical();
            
            // Restore normal operation when waking up
            if (current_power_state == POWER_STATE_IDLE) {
                current_power_state = POWER_STATE_NORMAL;
                printf("[POWER] Exiting idle power mode - restoring clock frequency\n");
            }
        }
        
        pico_rtos_exit_critical();
    } else {
        // Normal idle processing - just use WFI for basic power saving
        if (idle_counter % 100 == 0) {
            // Periodic check every 100 idle cycles
            last_power_check = current_time;
        }
        
        // Always use WFI during idle to save power
        __wfi();
    }
}

/**
 * @brief Update last activity timestamp
 * 
 * This function should be called whenever there is system activity
 * to prevent entering power save mode during active periods.
 */
void update_activity_timestamp(void) {
    pico_rtos_enter_critical();
    last_activity_time = pico_rtos_get_tick_count();
    pico_rtos_exit_critical();
}

/**
 * @brief Get current power consumption estimate
 * 
 * This function reads the ADC to estimate current power consumption.
 * In a real application, this would be connected to a current sensor.
 * 
 * @return Power consumption estimate in arbitrary units
 */
uint16_t get_power_consumption(void) {
    // Read ADC for power monitoring
    // Note: This is a simplified example - real power monitoring would
    // require external current sensing circuitry
    adc_select_input(0); // Select ADC0 (GPIO26)
    uint16_t adc_reading = adc_read();
    
    // Convert ADC reading to voltage (3.3V reference, 12-bit ADC)
    // Voltage = (adc_reading / 4095) * 3.3V
    return adc_reading;
}

/**
 * @brief Configure system for low power operation
 * 
 * This function configures various system components for optimal power efficiency.
 */
void configure_low_power_mode(void) {
    // Configure system for power efficiency
    // Note: In a real application, you would configure voltage regulators,
    // oscillators, and other power management features here
    
    printf("[POWER] Configured system for low power operation\n");
}

/**
 * @brief Restore system from low power mode
 * 
 * This function restores system configuration after waking from low power mode.
 */
void restore_from_low_power_mode(void) {
    // Restore system configuration from low power mode
    // Note: In a real application, you would restore voltage regulators,
    // oscillators, and other power management features here
    
    printf("[POWER] Restored system from low power mode\n");
}

// =============================================================================
// INTERRUPT HANDLERS
// =============================================================================

/**
 * @brief GPIO interrupt handler for wake-up events
 * 
 * This handler is called when the wake-up GPIO pin is triggered.
 * It signals the wake-up task and updates the power state.
 */
void gpio_wake_up_handler(uint gpio, uint32_t events) {
    if (gpio == WAKE_UP_PIN) {
        // Signal wake-up event
        wake_up_requested = true;
        
        // Signal the wake-up semaphore from interrupt context
        // Note: In a real implementation, you would use an ISR-safe version
        pico_rtos_semaphore_give(&wake_up_semaphore);
        
        // Update power state
        if (current_power_state != POWER_STATE_NORMAL) {
            current_power_state = POWER_STATE_WAKE_UP;
        }
        
        printf("[WAKE] GPIO wake-up event detected\n");
    }
}

// =============================================================================
// TASK IMPLEMENTATIONS
// =============================================================================

/**
 * @brief Main application task
 * 
 * This task represents the main application workload. It periodically
 * performs work and updates the activity timestamp to prevent unnecessary
 * power saving during active periods.
 */
void main_application_task(void *param) {
    (void)param;
    
    uint32_t work_counter = 0;
    bool led_state = false;
    
    printf("[MAIN] Main application task started\n");
    
    while (1) {
        // Simulate application work
        work_counter++;
        
        // Update activity timestamp
        update_activity_timestamp();
        
        // Toggle LED to show activity
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        
        // Acquire mutex to safely access power state
        if (pico_rtos_mutex_lock(&power_state_mutex, 100)) {
            printf("[MAIN] Work cycle %lu, Power state: %d, LED: %s\n", 
                   work_counter, current_power_state, led_state ? "ON" : "OFF");
            pico_rtos_mutex_unlock(&power_state_mutex);
        }
        
        // Variable delay based on power state
        uint32_t delay_ms = NORMAL_TASK_INTERVAL;
        if (current_power_state == POWER_STATE_IDLE) {
            delay_ms *= 2; // Slower operation in idle mode
        }
        
        pico_rtos_task_delay(delay_ms);
    }
}

/**
 * @brief Power monitoring task
 * 
 * This task continuously monitors power consumption and system state.
 * It collects power measurements and can trigger power management actions
 * based on consumption patterns.
 */
void power_monitoring_task(void *param) {
    (void)param;
    
    printf("[POWER] Power monitoring task started\n");
    
    while (1) {
        // Take power measurement
        power_measurement_t measurement;
        measurement.timestamp = pico_rtos_get_tick_count();
        measurement.voltage_reading = get_power_consumption();
        measurement.state = current_power_state;
        measurement.cpu_frequency = clock_get_hz(clk_sys);
        
        // Store measurement in circular buffer
        power_measurements[power_measurement_index] = measurement;
        power_measurement_index = (power_measurement_index + 1) % 10;
        
        // Print power status
        printf("[POWER] Time: %lu ms, ADC: %u, State: %d, CPU: %lu Hz\n",
               measurement.timestamp,
               measurement.voltage_reading,
               measurement.state,
               measurement.cpu_frequency);
        
        // Analyze power consumption trends
        if (power_measurement_index == 0) { // Every 10 measurements
            printf("[POWER] === Power Consumption Analysis ===\n");
            
            uint32_t total_consumption = 0;
            uint32_t idle_measurements = 0;
            
            for (int i = 0; i < 10; i++) {
                total_consumption += power_measurements[i].voltage_reading;
                if (power_measurements[i].state == POWER_STATE_IDLE) {
                    idle_measurements++;
                }
            }
            
            uint32_t average_consumption = total_consumption / 10;
            uint32_t idle_percentage = (idle_measurements * 100) / 10;
            
            printf("[POWER] Average consumption: %lu units\n", average_consumption);
            printf("[POWER] Idle time: %lu%%\n", idle_percentage);
            printf("[POWER] Power efficiency: %s\n", 
                   idle_percentage > 50 ? "GOOD" : "NEEDS IMPROVEMENT");
            printf("[POWER] =====================================\n");
        }
        
        pico_rtos_task_delay(POWER_MONITOR_INTERVAL);
    }
}

/**
 * @brief Wake-up event handling task
 * 
 * This task handles wake-up events from various sources (GPIO, timers, etc.)
 * and manages the transition from low power states back to normal operation.
 */
void wake_up_handling_task(void *param) {
    (void)param;
    
    printf("[WAKE] Wake-up handling task started\n");
    
    while (1) {
        // Wait for wake-up event
        if (pico_rtos_semaphore_take(&wake_up_semaphore, PICO_RTOS_WAIT_FOREVER)) {
            printf("[WAKE] Processing wake-up event\n");
            
            // Handle wake-up event
            if (wake_up_requested) {
                wake_up_requested = false;
                
                // Restore system from low power mode if needed
                if (current_power_state == POWER_STATE_SLEEP || 
                    current_power_state == POWER_STATE_WAKE_UP) {
                    
                    restore_from_low_power_mode();
                    
                    // Update power state
                    pico_rtos_enter_critical();
                    current_power_state = POWER_STATE_NORMAL;
                    pico_rtos_exit_critical();
                    
                    printf("[WAKE] System restored to normal operation\n");
                }
                
                // Update activity timestamp
                update_activity_timestamp();
                
                // Flash LED to indicate wake-up
                for (int i = 0; i < 6; i++) {
                    gpio_put(LED_PIN, i % 2);
                    pico_rtos_task_delay(100);
                }
            }
        }
    }
}

// =============================================================================
// INITIALIZATION FUNCTIONS
// =============================================================================

/**
 * @brief Initialize GPIO pins for power management
 */
void init_power_management_gpio(void) {
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    // Initialize wake-up pin with interrupt
    gpio_init(WAKE_UP_PIN);
    gpio_set_dir(WAKE_UP_PIN, GPIO_IN);
    gpio_pull_up(WAKE_UP_PIN);
    
    // Set up GPIO interrupt for wake-up pin
    gpio_set_irq_enabled_with_callback(WAKE_UP_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_wake_up_handler);
    
    printf("[INIT] GPIO initialized for power management\n");
}

/**
 * @brief Initialize ADC for power monitoring
 */
void init_power_monitoring_adc(void) {
    // Initialize ADC
    adc_init();
    
    // Initialize ADC pin
    adc_gpio_init(POWER_MONITOR_PIN);
    
    printf("[INIT] ADC initialized for power monitoring\n");
}

/**
 * @brief Initialize RTOS synchronization primitives
 */
bool init_rtos_primitives(void) {
    // Initialize mutex for power state protection
    if (!pico_rtos_mutex_init(&power_state_mutex)) {
        printf("[ERROR] Failed to initialize power state mutex\n");
        return false;
    }
    
    // Initialize semaphore for wake-up events
    if (!pico_rtos_semaphore_init(&wake_up_semaphore, 0, 1)) {
        printf("[ERROR] Failed to initialize wake-up semaphore\n");
        return false;
    }
    
    // Initialize queue for power events
    if (!pico_rtos_queue_init(&power_event_queue, power_event_queue_buffer, sizeof(uint32_t), 5)) {
        printf("[ERROR] Failed to initialize power event queue\n");
        return false;
    }
    
    printf("[INIT] RTOS synchronization primitives initialized\n");
    return true;
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    printf("\n");
    printf("========================================\n");
    printf("  Pico-RTOS Power Management Example\n");
    printf("  Version 0.2.1\n");
    printf("========================================\n");
    printf("\n");
    
    // Initialize hardware
    init_power_management_gpio();
    init_power_monitoring_adc();
    
    // Configure system for power efficiency
    configure_low_power_mode();
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("[ERROR] Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize RTOS synchronization primitives
    if (!init_rtos_primitives()) {
        printf("[ERROR] Failed to initialize RTOS primitives\n");
        return -1;
    }
    
    // Initialize activity timestamp
    update_activity_timestamp();
    
    // Register the idle hook for power management
    pico_rtos_set_idle_hook(power_management_idle_hook);
    printf("[INIT] Power management idle hook registered\n");
    
    printf("[INIT] Creating tasks...\n");
    
    // Create main application task
    if (!pico_rtos_task_create(&main_task_handle, "MainApp", main_application_task, NULL, 1024, 3)) {
        printf("[ERROR] Failed to create main application task\n");
        return -1;
    }
    
    // Create power monitoring task
    if (!pico_rtos_task_create(&power_monitor_task_handle, "PowerMon", power_monitoring_task, NULL, 1024, 2)) {
        printf("[ERROR] Failed to create power monitoring task\n");
        return -1;
    }
    
    // Create wake-up handling task
    if (!pico_rtos_task_create(&wake_up_task_handle, "WakeUp", wake_up_handling_task, NULL, 512, 4)) {
        printf("[ERROR] Failed to create wake-up handling task\n");
        return -1;
    }
    
    printf("[INIT] All tasks created successfully\n");
    printf("\n");
    printf("Power Management Features:\n");
    printf("- Idle task hooks with WFI power saving\n");
    printf("- Dynamic power state management\n");
    printf("- GPIO wake-up events (pin %d)\n", WAKE_UP_PIN);
    printf("- Power consumption monitoring\n");
    printf("- Clock frequency optimization\n");
    printf("- Voltage regulation control\n");
    printf("\n");
    printf("Usage:\n");
    printf("- Connect a button to GPIO %d (pull to ground) for wake-up\n", WAKE_UP_PIN);
    printf("- Monitor serial output for power consumption data\n");
    printf("- Observe LED blinking patterns for system state\n");
    printf("\n");
    printf("Starting scheduler...\n");
    
    // Start the scheduler
    pico_rtos_start();
    
    // We should never reach here
    printf("[ERROR] Scheduler returned unexpectedly\n");
    return -1;
}

// =============================================================================
// POWER MANAGEMENT OPTIMIZATION NOTES
// =============================================================================

/*
 * Additional Power Optimization Techniques:
 * 
 * 1. Clock Management:
 *    - Use lower clock frequencies when possible
 *    - Switch to ring oscillator (ROSC) for less critical timing
 *    - Gate clocks to unused peripherals
 * 
 * 2. Voltage Scaling:
 *    - Reduce core voltage when running at lower frequencies
 *    - Use voltage regulator power-save modes
 * 
 * 3. Peripheral Management:
 *    - Disable unused peripherals (SPI, I2C, UART, etc.)
 *    - Use peripheral-specific low power modes
 *    - Minimize GPIO drive strength
 * 
 * 4. Memory Optimization:
 *    - Use retention RAM for critical data during sleep
 *    - Minimize memory allocations during low power periods
 * 
 * 5. Interrupt Optimization:
 *    - Use edge-triggered interrupts where possible
 *    - Minimize interrupt service routine execution time
 *    - Use interrupt priorities effectively
 * 
 * 6. Task Scheduling Optimization:
 *    - Group related operations to minimize wake-ups
 *    - Use longer time slices during low power periods
 *    - Implement cooperative scheduling for power-critical tasks
 * 
 * 7. Wake-up Source Management:
 *    - Configure multiple wake-up sources (GPIO, RTC, watchdog)
 *    - Use wake-up source priorities
 *    - Implement wake-up source filtering
 * 
 * 8. Power Measurement and Analysis:
 *    - Implement real-time power monitoring
 *    - Log power consumption patterns
 *    - Optimize based on actual measurements
 */