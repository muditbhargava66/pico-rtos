/**
 * @file main.c
 * @brief Event Group Multi-Event Coordination Example for Pico-RTOS v0.3.1
 * 
 * This example demonstrates advanced synchronization using event groups to coordinate
 * multiple events across different tasks. It showcases:
 * 
 * 1. Multi-event coordination with "wait for any" and "wait for all" semantics
 * 2. System initialization synchronization across multiple subsystems
 * 3. Complex state machine coordination using event groups
 * 4. Error handling and timeout management with event groups
 * 5. Performance monitoring and statistics collection
 * 
 * The example simulates a sensor data acquisition system with multiple sensors,
 * data processing, and communication subsystems that must coordinate their activities.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"

// Disable migration warnings for examples (they already use v0.3.1 APIs)
#define PICO_RTOS_DISABLE_MIGRATION_WARNINGS
#include "pico_rtos.h"

// =============================================================================
// CONFIGURATION AND EVENT DEFINITIONS
// =============================================================================

#define LED_PIN 25

// System initialization events
#define EVENT_SENSOR_INIT_COMPLETE    (1 << 0)
#define EVENT_COMM_INIT_COMPLETE      (1 << 1)
#define EVENT_STORAGE_INIT_COMPLETE   (1 << 2)
#define EVENT_SYSTEM_READY           (1 << 3)

// Data processing events
#define EVENT_SENSOR_DATA_READY       (1 << 4)
#define EVENT_CALIBRATION_COMPLETE    (1 << 5)
#define EVENT_PROCESSING_COMPLETE     (1 << 6)
#define EVENT_DATA_VALIDATED         (1 << 7)

// Communication events
#define EVENT_NETWORK_CONNECTED       (1 << 8)
#define EVENT_DATA_TRANSMITTED        (1 << 9)
#define EVENT_ACK_RECEIVED           (1 << 10)
#define EVENT_TRANSMISSION_ERROR     (1 << 11)

// System control events
#define EVENT_SHUTDOWN_REQUESTED     (1 << 12)
#define EVENT_EMERGENCY_STOP         (1 << 13)
#define EVENT_MAINTENANCE_MODE       (1 << 14)
#define EVENT_SYSTEM_ERROR           (1 << 15)

// Convenience event masks
#define EVENTS_ALL_INIT_COMPLETE     (EVENT_SENSOR_INIT_COMPLETE | EVENT_COMM_INIT_COMPLETE | EVENT_STORAGE_INIT_COMPLETE)
#define EVENTS_DATA_PROCESSING       (EVENT_SENSOR_DATA_READY | EVENT_CALIBRATION_COMPLETE)
#define EVENTS_COMMUNICATION_OK      (EVENT_NETWORK_CONNECTED | EVENT_ACK_RECEIVED)
#define EVENTS_SYSTEM_SHUTDOWN       (EVENT_SHUTDOWN_REQUESTED | EVENT_EMERGENCY_STOP)

// =============================================================================
// DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    float pressure;
    uint32_t timestamp;
    bool valid;
} sensor_data_t;

typedef struct {
    uint32_t total_samples;
    uint32_t valid_samples;
    uint32_t processing_cycles;
    uint32_t transmission_attempts;
    uint32_t successful_transmissions;
    uint32_t initialization_time_ms;
} system_stats_t;

typedef enum {
    SYSTEM_STATE_INITIALIZING,
    SYSTEM_STATE_CALIBRATING,
    SYSTEM_STATE_RUNNING,
    SYSTEM_STATE_MAINTENANCE,
    SYSTEM_STATE_SHUTDOWN,
    SYSTEM_STATE_ERROR
} system_state_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Event groups for different coordination scenarios
static pico_rtos_event_group_t system_events;
static pico_rtos_event_group_t data_events;
static pico_rtos_event_group_t comm_events;

// System state and statistics
static volatile system_state_t current_system_state = SYSTEM_STATE_INITIALIZING;
static system_stats_t system_stats = {0};
static sensor_data_t latest_sensor_data = {0};

// Synchronization primitives
static pico_rtos_mutex_t stats_mutex;
static pico_rtos_mutex_t sensor_data_mutex;

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Update system statistics in a thread-safe manner
 */
void update_stats(void (*update_func)(system_stats_t *stats)) {
    if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
        update_func(&system_stats);
        pico_rtos_mutex_unlock(&stats_mutex);
    }
}

/**
 * @brief Get current system statistics
 */
system_stats_t get_system_stats(void) {
    system_stats_t stats = {0};
    if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
        stats = system_stats;
        pico_rtos_mutex_unlock(&stats_mutex);
    }
    return stats;
}

/**
 * @brief Simulate sensor reading with some variability
 */
sensor_data_t simulate_sensor_reading(uint32_t sensor_id) {
    sensor_data_t data;
    uint32_t tick = pico_rtos_get_tick_count();
    
    data.sensor_id = sensor_id;
    data.timestamp = tick;
    
    // Simulate sensor readings with some variation
    data.temperature = 20.0f + (float)(tick % 100) / 10.0f + sinf((float)tick / 1000.0f) * 5.0f;
    data.humidity = 45.0f + (float)(tick % 200) / 20.0f + cosf((float)tick / 1500.0f) * 10.0f;
    data.pressure = 1013.25f + sinf((float)tick / 2000.0f) * 20.0f;
    
    // Occasionally simulate invalid readings
    data.valid = (tick % 47) != 0;  // ~2% invalid readings
    
    return data;
}

// =============================================================================
// SYSTEM INITIALIZATION TASKS
// =============================================================================

/**
 * @brief Sensor subsystem initialization task
 * 
 * Demonstrates:
 * - Simulated initialization sequence with delays
 * - Event signaling upon completion
 * - Error handling during initialization
 */
void sensor_init_task(void *param) {
    printf("[Sensor Init] Starting sensor subsystem initialization\n");
    
    // Simulate sensor hardware initialization
    printf("[Sensor Init] Initializing sensor hardware...\n");
    pico_rtos_task_delay(800);  // Simulate initialization time
    
    printf("[Sensor Init] Configuring sensor parameters...\n");
    pico_rtos_task_delay(400);
    
    printf("[Sensor Init] Running sensor self-test...\n");
    pico_rtos_task_delay(600);
    
    // Simulate occasional initialization failure
    if ((pico_rtos_get_tick_count() % 23) == 0) {
        printf("[Sensor Init] ERROR: Sensor initialization failed!\n");
        pico_rtos_event_group_set_bits(&system_events, EVENT_SYSTEM_ERROR);
        return;
    }
    
    printf("[Sensor Init] Sensor subsystem initialization complete\n");
    
    // Signal initialization completion
    pico_rtos_event_group_set_bits(&system_events, EVENT_SENSOR_INIT_COMPLETE);
    
    // Task completes after initialization
}

/**
 * @brief Communication subsystem initialization task
 */
void comm_init_task(void *param) {
    printf("[Comm Init] Starting communication subsystem initialization\n");
    
    printf("[Comm Init] Initializing network interface...\n");
    pico_rtos_task_delay(1200);
    
    printf("[Comm Init] Establishing network connection...\n");
    pico_rtos_task_delay(800);
    
    printf("[Comm Init] Configuring communication protocols...\n");
    pico_rtos_task_delay(300);
    
    printf("[Comm Init] Communication subsystem initialization complete\n");
    
    // Signal initialization completion
    pico_rtos_event_group_set_bits(&system_events, EVENT_COMM_INIT_COMPLETE);
    
    // Simulate network connection establishment
    pico_rtos_task_delay(500);
    pico_rtos_event_group_set_bits(&comm_events, EVENT_NETWORK_CONNECTED);
}

/**
 * @brief Storage subsystem initialization task
 */
void storage_init_task(void *param) {
    printf("[Storage Init] Starting storage subsystem initialization\n");
    
    printf("[Storage Init] Initializing storage device...\n");
    pico_rtos_task_delay(600);
    
    printf("[Storage Init] Checking storage integrity...\n");
    pico_rtos_task_delay(900);
    
    printf("[Storage Init] Creating data directories...\n");
    pico_rtos_task_delay(200);
    
    printf("[Storage Init] Storage subsystem initialization complete\n");
    
    // Signal initialization completion
    pico_rtos_event_group_set_bits(&system_events, EVENT_STORAGE_INIT_COMPLETE);
}

// =============================================================================
// SYSTEM COORDINATION TASK
// =============================================================================

/**
 * @brief System coordinator task - manages overall system state
 * 
 * Demonstrates:
 * - Waiting for multiple initialization events with "wait for all" semantics
 * - State machine coordination using event groups
 * - Timeout handling for initialization
 * - System state transitions based on events
 */
void system_coordinator_task(void *param) {
    uint32_t init_start_time = pico_rtos_get_tick_count();
    
    printf("[Coordinator] System coordinator started\n");
    printf("[Coordinator] Waiting for all subsystems to initialize...\n");
    
    // Wait for all initialization events with timeout
    uint32_t init_events = pico_rtos_event_group_wait_bits(&system_events,
                                                           EVENTS_ALL_INIT_COMPLETE,
                                                           true,    // Wait for ALL events
                                                           false,   // Don't clear on exit
                                                           10000);  // 10 second timeout
    
    if (init_events == EVENTS_ALL_INIT_COMPLETE) {
        uint32_t init_time = pico_rtos_get_tick_count() - init_start_time;
        printf("[Coordinator] All subsystems initialized successfully in %lu ms\n", init_time);
        
        // Update initialization time in statistics
        if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
            system_stats.initialization_time_ms = pico_rtos_get_tick_count() - init_start_time;
            pico_rtos_mutex_unlock(&stats_mutex);
        }
        
        // Signal system ready
        pico_rtos_event_group_set_bits(&system_events, EVENT_SYSTEM_READY);
        current_system_state = SYSTEM_STATE_CALIBRATING;
        
    } else {
        printf("[Coordinator] ERROR: System initialization timeout or failure\n");
        printf("[Coordinator] Received events: 0x%08lX, Expected: 0x%08lX\n", 
               init_events, EVENTS_ALL_INIT_COMPLETE);
        
        current_system_state = SYSTEM_STATE_ERROR;
        pico_rtos_event_group_set_bits(&system_events, EVENT_SYSTEM_ERROR);
        return;
    }
    
    // Main coordination loop
    while (1) {
        // Wait for any system control event
        uint32_t events = pico_rtos_event_group_wait_bits(&system_events,
                                                          EVENTS_SYSTEM_SHUTDOWN | EVENT_SYSTEM_ERROR | EVENT_MAINTENANCE_MODE,
                                                          false,   // Wait for ANY event
                                                          true,    // Clear on exit
                                                          5000);   // 5 second timeout
        
        if (events & EVENT_EMERGENCY_STOP) {
            printf("[Coordinator] EMERGENCY STOP activated!\n");
            current_system_state = SYSTEM_STATE_SHUTDOWN;
            // Trigger shutdown sequence
            break;
            
        } else if (events & EVENT_SHUTDOWN_REQUESTED) {
            printf("[Coordinator] Graceful shutdown requested\n");
            current_system_state = SYSTEM_STATE_SHUTDOWN;
            // Trigger graceful shutdown
            break;
            
        } else if (events & EVENT_SYSTEM_ERROR) {
            printf("[Coordinator] System error detected, entering error state\n");
            current_system_state = SYSTEM_STATE_ERROR;
            
        } else if (events & EVENT_MAINTENANCE_MODE) {
            printf("[Coordinator] Entering maintenance mode\n");
            current_system_state = SYSTEM_STATE_MAINTENANCE;
            
        } else if (events == 0) {
            // Timeout - normal operation
            if (current_system_state == SYSTEM_STATE_CALIBRATING) {
                printf("[Coordinator] Calibration period complete, entering normal operation\n");
                current_system_state = SYSTEM_STATE_RUNNING;
                pico_rtos_event_group_set_bits(&data_events, EVENT_CALIBRATION_COMPLETE);
            }
        }
    }
    
    printf("[Coordinator] System coordinator shutting down\n");
}

// =============================================================================
// DATA PROCESSING TASKS
// =============================================================================

/**
 * @brief Sensor data acquisition task
 * 
 * Demonstrates:
 * - Waiting for system ready event before starting
 * - Periodic data generation with event signaling
 * - Thread-safe data sharing
 */
void sensor_data_task(void *param) {
    printf("[Sensor Data] Waiting for system to be ready...\n");
    
    // Wait for system ready event
    uint32_t events = pico_rtos_event_group_wait_bits(&system_events,
                                                      EVENT_SYSTEM_READY,
                                                      false,   // Wait for this event
                                                      false,   // Don't clear
                                                      15000);  // 15 second timeout
    
    if (!(events & EVENT_SYSTEM_READY)) {
        printf("[Sensor Data] ERROR: System ready timeout\n");
        return;
    }
    
    printf("[Sensor Data] Starting sensor data acquisition\n");
    
    while (current_system_state != SYSTEM_STATE_SHUTDOWN && 
           current_system_state != SYSTEM_STATE_ERROR) {
        
        // Generate sensor data
        sensor_data_t data = simulate_sensor_reading(1);
        
        // Update shared sensor data
        if (pico_rtos_mutex_lock(&sensor_data_mutex, 100)) {
            latest_sensor_data = data;
            pico_rtos_mutex_unlock(&sensor_data_mutex);
        }
        
        // Update statistics
        if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
            system_stats.total_samples++;
            if (data.valid) {
                system_stats.valid_samples++;
            }
            pico_rtos_mutex_unlock(&stats_mutex);
        }
        
        // Signal new data available
        pico_rtos_event_group_set_bits(&data_events, EVENT_SENSOR_DATA_READY);
        
        printf("[Sensor Data] New sensor reading: T=%.1f°C, H=%.1f%%, P=%.1fhPa %s\n",
               data.temperature, data.humidity, data.pressure,
               data.valid ? "(valid)" : "(INVALID)");
        
        pico_rtos_task_delay(2000);  // 2 second sampling interval
    }
    
    printf("[Sensor Data] Sensor data acquisition stopped\n");
}

/**
 * @brief Data processing task
 * 
 * Demonstrates:
 * - Waiting for multiple events (sensor data + calibration complete)
 * - Complex event coordination for data processing pipeline
 * - Event-driven processing workflow
 */
void data_processing_task(void *param) {
    printf("[Data Processing] Data processing task started\n");
    
    while (current_system_state != SYSTEM_STATE_SHUTDOWN && 
           current_system_state != SYSTEM_STATE_ERROR) {
        
        // Wait for both sensor data and calibration to be complete
        uint32_t events = pico_rtos_event_group_wait_bits(&data_events,
                                                          EVENTS_DATA_PROCESSING,
                                                          true,    // Wait for ALL events
                                                          true,    // Clear on exit
                                                          5000);   // 5 second timeout
        
        if (events == EVENTS_DATA_PROCESSING) {
            // Get current sensor data
            sensor_data_t data = {0};
            if (pico_rtos_mutex_lock(&sensor_data_mutex, 100)) {
                data = latest_sensor_data;
                pico_rtos_mutex_unlock(&sensor_data_mutex);
            }
            
            if (data.valid) {
                printf("[Data Processing] Processing sensor data from sensor %lu\n", data.sensor_id);
                
                // Simulate data processing
                pico_rtos_task_delay(300);
                
                // Simulate data validation
                bool validation_passed = (data.temperature > -40.0f && data.temperature < 85.0f) &&
                                       (data.humidity >= 0.0f && data.humidity <= 100.0f) &&
                                       (data.pressure > 800.0f && data.pressure < 1200.0f);
                
                if (validation_passed) {
                    printf("[Data Processing] Data validation passed\n");
                    pico_rtos_event_group_set_bits(&data_events, EVENT_DATA_VALIDATED);
                    pico_rtos_event_group_set_bits(&data_events, EVENT_PROCESSING_COMPLETE);
                } else {
                    printf("[Data Processing] Data validation FAILED\n");
                }
                
                // Update processing statistics
                if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
                    system_stats.processing_cycles++;
                    pico_rtos_mutex_unlock(&stats_mutex);
                }
                
            } else {
                printf("[Data Processing] Skipping invalid sensor data\n");
            }
            
        } else if (events == 0) {
            // Timeout - check if we're still waiting for calibration
            if (current_system_state == SYSTEM_STATE_CALIBRATING) {
                printf("[Data Processing] Waiting for system calibration to complete...\n");
            }
        }
    }
    
    printf("[Data Processing] Data processing stopped\n");
}

// =============================================================================
// COMMUNICATION TASKS
// =============================================================================

/**
 * @brief Data transmission task
 * 
 * Demonstrates:
 * - Complex event coordination for transmission workflow
 * - Error handling and retry logic with events
 * - Multi-step communication protocol using events
 */
void data_transmission_task(void *param) {
    printf("[Data Transmission] Data transmission task started\n");
    
    while (current_system_state != SYSTEM_STATE_SHUTDOWN && 
           current_system_state != SYSTEM_STATE_ERROR) {
        
        // Wait for processed and validated data
        uint32_t events = pico_rtos_event_group_wait_bits(&data_events,
                                                          EVENT_DATA_VALIDATED,
                                                          false,   // Wait for this event
                                                          true,    // Clear on exit
                                                          10000);  // 10 second timeout
        
        if (events & EVENT_DATA_VALIDATED) {
            // Check if network is connected
            uint32_t comm_status = pico_rtos_event_group_get_bits(&comm_events);
            
            if (comm_status & EVENT_NETWORK_CONNECTED) {
                printf("[Data Transmission] Transmitting validated data...\n");
                
                // Simulate data transmission
                pico_rtos_task_delay(500);
                
                // Update transmission statistics
                if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
                    system_stats.transmission_attempts++;
                    pico_rtos_mutex_unlock(&stats_mutex);
                }
                
                // Simulate transmission success/failure
                bool transmission_success = (pico_rtos_get_tick_count() % 17) != 0;  // ~6% failure rate
                
                if (transmission_success) {
                    printf("[Data Transmission] Data transmitted successfully\n");
                    pico_rtos_event_group_set_bits(&comm_events, EVENT_DATA_TRANSMITTED);
                    
                    // Wait for acknowledgment
                    uint32_t ack_events = pico_rtos_event_group_wait_bits(&comm_events,
                                                                         EVENT_ACK_RECEIVED | EVENT_TRANSMISSION_ERROR,
                                                                         false,   // Wait for ANY
                                                                         true,    // Clear on exit
                                                                         3000);   // 3 second timeout
                    
                    if (ack_events & EVENT_ACK_RECEIVED) {
                        printf("[Data Transmission] Acknowledgment received\n");
                        if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
                            system_stats.successful_transmissions++;
                            pico_rtos_mutex_unlock(&stats_mutex);
                        }
                    } else {
                        printf("[Data Transmission] Acknowledgment timeout\n");
                    }
                    
                } else {
                    printf("[Data Transmission] Transmission failed\n");
                    pico_rtos_event_group_set_bits(&comm_events, EVENT_TRANSMISSION_ERROR);
                }
                
            } else {
                printf("[Data Transmission] Network not connected, skipping transmission\n");
            }
        }
    }
    
    printf("[Data Transmission] Data transmission stopped\n");
}

/**
 * @brief Communication status simulation task
 * 
 * Simulates network events and acknowledgments
 */
void comm_status_task(void *param) {
    bool network_connected = true;
    uint32_t ack_counter = 0;
    
    printf("[Comm Status] Communication status simulation started\n");
    
    while (current_system_state != SYSTEM_STATE_SHUTDOWN && 
           current_system_state != SYSTEM_STATE_ERROR) {
        
        // Simulate network connection changes
        if ((pico_rtos_get_tick_count() % 30000) < 1000) {  // Disconnect for ~1 second every 30 seconds
            if (network_connected) {
                printf("[Comm Status] Network disconnected\n");
                pico_rtos_event_group_clear_bits(&comm_events, EVENT_NETWORK_CONNECTED);
                network_connected = false;
            }
        } else {
            if (!network_connected) {
                printf("[Comm Status] Network reconnected\n");
                pico_rtos_event_group_set_bits(&comm_events, EVENT_NETWORK_CONNECTED);
                network_connected = true;
            }
        }
        
        // Check for data transmission events and simulate acknowledgments
        uint32_t comm_status = pico_rtos_event_group_get_bits(&comm_events);
        if (comm_status & EVENT_DATA_TRANSMITTED) {
            pico_rtos_event_group_clear_bits(&comm_events, EVENT_DATA_TRANSMITTED);
            
            // Simulate acknowledgment delay
            pico_rtos_task_delay(200 + (ack_counter % 300));
            
            // Simulate occasional acknowledgment failure
            if ((ack_counter % 19) != 0) {  // ~5% ack failure rate
                pico_rtos_event_group_set_bits(&comm_events, EVENT_ACK_RECEIVED);
            } else {
                pico_rtos_event_group_set_bits(&comm_events, EVENT_TRANSMISSION_ERROR);
            }
            
            ack_counter++;
        }
        
        pico_rtos_task_delay(100);
    }
    
    printf("[Comm Status] Communication status simulation stopped\n");
}

// =============================================================================
// MONITORING AND CONTROL TASKS
// =============================================================================

/**
 * @brief System monitoring task
 * 
 * Demonstrates:
 * - Periodic system status monitoring
 * - Event-based system control
 * - Statistics reporting
 */
void system_monitor_task(void *param) {
    uint32_t monitor_cycle = 0;
    
    printf("[System Monitor] System monitoring started\n");
    
    while (current_system_state != SYSTEM_STATE_SHUTDOWN) {
        monitor_cycle++;
        
        // Get current system statistics
        system_stats_t stats = get_system_stats();
        
        printf("\n=== SYSTEM STATUS REPORT #%lu ===\n", monitor_cycle);
        printf("System State: ");
        switch (current_system_state) {
            case SYSTEM_STATE_INITIALIZING: printf("INITIALIZING"); break;
            case SYSTEM_STATE_CALIBRATING: printf("CALIBRATING"); break;
            case SYSTEM_STATE_RUNNING: printf("RUNNING"); break;
            case SYSTEM_STATE_MAINTENANCE: printf("MAINTENANCE"); break;
            case SYSTEM_STATE_SHUTDOWN: printf("SHUTDOWN"); break;
            case SYSTEM_STATE_ERROR: printf("ERROR"); break;
        }
        printf("\n");
        
        printf("Uptime: %lu ms\n", pico_rtos_get_uptime_ms());
        printf("Initialization Time: %lu ms\n", stats.initialization_time_ms);
        printf("Sensor Samples: %lu total, %lu valid (%.1f%% valid)\n",
               stats.total_samples, stats.valid_samples,
               stats.total_samples > 0 ? (float)stats.valid_samples / stats.total_samples * 100.0f : 0.0f);
        printf("Processing Cycles: %lu\n", stats.processing_cycles);
        printf("Transmissions: %lu attempts, %lu successful (%.1f%% success)\n",
               stats.transmission_attempts, stats.successful_transmissions,
               stats.transmission_attempts > 0 ? (float)stats.successful_transmissions / stats.transmission_attempts * 100.0f : 0.0f);
        
        // Display current event states
        uint32_t system_events_state = pico_rtos_event_group_get_bits(&system_events);
        uint32_t data_events_state = pico_rtos_event_group_get_bits(&data_events);
        uint32_t comm_events_state = pico_rtos_event_group_get_bits(&comm_events);
        
        printf("Active Events:\n");
        printf("  System: 0x%08lX\n", system_events_state);
        printf("  Data: 0x%08lX\n", data_events_state);
        printf("  Communication: 0x%08lX\n", comm_events_state);
        printf("=====================================\n\n");
        
        // Simulate occasional maintenance mode trigger
        if (monitor_cycle == 20) {
            printf("[System Monitor] Triggering maintenance mode for demonstration\n");
            pico_rtos_event_group_set_bits(&system_events, EVENT_MAINTENANCE_MODE);
        }
        
        // Simulate shutdown after extended operation
        if (monitor_cycle == 40) {
            printf("[System Monitor] Triggering graceful shutdown for demonstration\n");
            pico_rtos_event_group_set_bits(&system_events, EVENT_SHUTDOWN_REQUESTED);
        }
        
        pico_rtos_task_delay(5000);  // Report every 5 seconds
    }
    
    printf("[System Monitor] System monitoring stopped\n");
}

/**
 * @brief LED status indicator task
 */
void led_status_task(void *param) {
    bool led_state = false;
    uint32_t blink_rate = 1000;  // Default 1 second
    
    while (current_system_state != SYSTEM_STATE_SHUTDOWN) {
        // Adjust blink rate based on system state
        switch (current_system_state) {
            case SYSTEM_STATE_INITIALIZING:
                blink_rate = 200;  // Fast blink during init
                break;
            case SYSTEM_STATE_CALIBRATING:
                blink_rate = 500;  // Medium blink during calibration
                break;
            case SYSTEM_STATE_RUNNING:
                blink_rate = 1000; // Slow blink during normal operation
                break;
            case SYSTEM_STATE_MAINTENANCE:
                blink_rate = 100;  // Very fast blink in maintenance
                break;
            case SYSTEM_STATE_ERROR:
                blink_rate = 50;   // Extremely fast blink for errors
                break;
            default:
                blink_rate = 1000;
                break;
        }
        
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        pico_rtos_task_delay(blink_rate / 2);
    }
    
    // Turn off LED on shutdown
    gpio_put(LED_PIN, 0);
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize GPIO for LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    printf("\n=== Pico-RTOS v0.3.1 Event Group Multi-Event Coordination Example ===\n");
    printf("This example demonstrates:\n");
    printf("1. Multi-event coordination with 'wait for any' and 'wait for all' semantics\n");
    printf("2. System initialization synchronization across multiple subsystems\n");
    printf("3. Complex state machine coordination using event groups\n");
    printf("4. Error handling and timeout management with event groups\n");
    printf("5. Performance monitoring and statistics collection\n\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize event groups
    printf("Initializing event groups...\n");
    if (!pico_rtos_event_group_init(&system_events) ||
        !pico_rtos_event_group_init(&data_events) ||
        !pico_rtos_event_group_init(&comm_events)) {
        printf("ERROR: Failed to initialize event groups\n");
        return -1;
    }
    
    // Initialize synchronization primitives
    if (!pico_rtos_mutex_init(&stats_mutex) ||
        !pico_rtos_mutex_init(&sensor_data_mutex)) {
        printf("ERROR: Failed to initialize mutexes\n");
        return -1;
    }
    
    printf("Event groups and synchronization primitives initialized\n\n");
    
    // Create initialization tasks
    printf("Creating initialization tasks...\n");
    static pico_rtos_task_t sensor_init_task_handle, comm_init_task_handle, storage_init_task_handle;
    
    if (!pico_rtos_task_create(&sensor_init_task_handle, "SensorInit", sensor_init_task, NULL, 1024, 6) ||
        !pico_rtos_task_create(&comm_init_task_handle, "CommInit", comm_init_task, NULL, 1024, 6) ||
        !pico_rtos_task_create(&storage_init_task_handle, "StorageInit", storage_init_task, NULL, 1024, 6)) {
        printf("ERROR: Failed to create initialization tasks\n");
        return -1;
    }
    
    // Create system coordination task
    static pico_rtos_task_t coordinator_task_handle;
    if (!pico_rtos_task_create(&coordinator_task_handle, "Coordinator", system_coordinator_task, NULL, 1024, 7)) {
        printf("ERROR: Failed to create system coordinator task\n");
        return -1;
    }
    
    // Create data processing tasks
    printf("Creating data processing tasks...\n");
    static pico_rtos_task_t sensor_data_task_handle, data_processing_task_handle;
    
    if (!pico_rtos_task_create(&sensor_data_task_handle, "SensorData", sensor_data_task, NULL, 1024, 4) ||
        !pico_rtos_task_create(&data_processing_task_handle, "DataProcessing", data_processing_task, NULL, 1024, 3)) {
        printf("ERROR: Failed to create data processing tasks\n");
        return -1;
    }
    
    // Create communication tasks
    printf("Creating communication tasks...\n");
    static pico_rtos_task_t data_transmission_task_handle, comm_status_task_handle;
    
    if (!pico_rtos_task_create(&data_transmission_task_handle, "DataTransmission", data_transmission_task, NULL, 1024, 3) ||
        !pico_rtos_task_create(&comm_status_task_handle, "CommStatus", comm_status_task, NULL, 1024, 2)) {
        printf("ERROR: Failed to create communication tasks\n");
        return -1;
    }
    
    // Create monitoring tasks
    printf("Creating monitoring tasks...\n");
    static pico_rtos_task_t monitor_task_handle, led_task_handle;
    
    if (!pico_rtos_task_create(&monitor_task_handle, "SystemMonitor", system_monitor_task, NULL, 1024, 1) ||
        !pico_rtos_task_create(&led_task_handle, "LEDStatus", led_status_task, NULL, 256, 1)) {
        printf("ERROR: Failed to create monitoring tasks\n");
        return -1;
    }
    
    printf("All tasks created successfully\n");
    printf("Starting scheduler...\n\n");
    
    // Start the RTOS scheduler
    pico_rtos_start();
    
    // Should never reach here
    printf("ERROR: Scheduler returned unexpectedly\n");
    return -1;
}