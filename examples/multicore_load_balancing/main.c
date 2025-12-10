/**
 * @file main.c
 * @brief Multi-Core Load Balancing and IPC Example for Pico-RTOS v0.3.1
 * 
 * This example demonstrates multi-core capabilities including:
 * 
 * 1. Task distribution across both RP2040 cores
 * 2. Core affinity assignment and load balancing
 * 3. Inter-core communication using IPC messages
 * 4. Core-specific resource management
 * 5. Performance monitoring across cores
 * 6. Graceful handling of core failures
 * 
 * The example simulates a dual-core data processing system where:
 * - Core 0: Handles sensor data acquisition and system control
 * - Core 1: Handles data processing and communication
 * - Both cores: Share workload through load balancing
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"

// Disable migration warnings for examples (they already use v0.3.1 APIs)
#define PICO_RTOS_DISABLE_MIGRATION_WARNINGS
#include "pico_rtos.h"

// =============================================================================
// CONFIGURATION AND MESSAGE DEFINITIONS
// =============================================================================

#define LED_PIN 25

// IPC Message types
#define IPC_MSG_SENSOR_DATA     1
#define IPC_MSG_PROCESSED_DATA  2
#define IPC_MSG_CONTROL_CMD     3
#define IPC_MSG_STATUS_UPDATE   4
#define IPC_MSG_HEARTBEAT       5

// Core assignment strategy
#define CORE_ASSIGNMENT_FIXED       0
#define CORE_ASSIGNMENT_BALANCED    1

// =============================================================================
// DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    float pressure;
    uint32_t timestamp;
    uint32_t sequence;
} sensor_data_t;

typedef struct {
    uint32_t processing_stage;
    float processed_values[4];
    uint32_t quality_score;
    uint32_t processing_time_us;
} processed_data_t;

typedef struct {
    uint32_t command_id;
    uint32_t target_core;
    uint32_t parameters[2];
} control_command_t;

typedef struct {
    uint32_t core_id;
    uint32_t cpu_usage_percent;
    uint32_t active_tasks;
    uint32_t messages_processed;
    uint32_t uptime_ms;
} status_update_t;

typedef struct {
    uint32_t core_id;
    uint32_t timestamp;
    uint32_t sequence;
} heartbeat_t;

// Performance tracking per core
typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t processing_cycles;
    uint32_t total_processing_time_us;
    uint32_t max_processing_time_us;
    uint32_t min_processing_time_us;
    uint32_t ipc_failures;
} core_performance_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Performance tracking
static core_performance_t core0_performance = {0};
static core_performance_t core1_performance = {0};

// Synchronization
static pico_rtos_mutex_t performance_mutex;

// System control
static volatile bool system_running = true;
static volatile uint32_t core_assignment_strategy = CORE_ASSIGNMENT_BALANCED;

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Update core performance statistics
 */
void update_core_performance(uint32_t core_id, uint32_t processing_time_us, bool is_send, bool success) {
    if (pico_rtos_mutex_lock(&performance_mutex, 100)) {
        core_performance_t *perf = (core_id == 0) ? &core0_performance : &core1_performance;
        
        if (is_send) {
            if (success) {
                perf->messages_sent++;
            } else {
                perf->ipc_failures++;
            }
        } else {
            if (success) {
                perf->messages_received++;
                perf->processing_cycles++;
                perf->total_processing_time_us += processing_time_us;
                
                if (processing_time_us > perf->max_processing_time_us) {
                    perf->max_processing_time_us = processing_time_us;
                }
                if (perf->min_processing_time_us == 0 || processing_time_us < perf->min_processing_time_us) {
                    perf->min_processing_time_us = processing_time_us;
                }
            }
        }
        
        pico_rtos_mutex_unlock(&performance_mutex);
    }
}

/**
 * @brief Send IPC message with performance tracking
 */
bool send_ipc_message_tracked(uint32_t target_core, uint32_t message_type, const void *data, 
                             uint32_t data_length, uint32_t timeout) {
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    // Create IPC message structure
    pico_rtos_ipc_message_t message;
    message.type = (pico_rtos_ipc_message_type_t)(PICO_RTOS_IPC_MSG_USER_DEFINED + message_type);
    message.source_core = current_core;
    message.data1 = (uint32_t)data;  // In a real implementation, you'd copy the data
    message.data2 = data_length;
    message.timestamp = pico_rtos_get_tick_count();
    
    bool success = pico_rtos_ipc_send_message(target_core, &message, timeout);
    update_core_performance(current_core, 0, true, success);
    
    return success;
}

/**
 * @brief Simulate sensor reading
 */
sensor_data_t generate_sensor_data(uint32_t sensor_id, uint32_t sequence) {
    sensor_data_t data;
    uint32_t tick = pico_rtos_get_tick_count();
    
    data.sensor_id = sensor_id;
    data.sequence = sequence;
    data.timestamp = tick;
    
    // Generate realistic sensor data with some variation
    data.temperature = 22.0f + sinf((float)tick / 1000.0f) * 8.0f + (float)(tick % 50) / 50.0f;
    data.humidity = 55.0f + cosf((float)tick / 1500.0f) * 15.0f + (float)(tick % 30) / 30.0f;
    data.pressure = 1013.25f + sinf((float)tick / 2000.0f) * 25.0f;
    
    return data;
}

/**
 * @brief Process sensor data (simulate complex processing)
 */
processed_data_t process_sensor_data(const sensor_data_t *sensor_data) {
    processed_data_t result;
    uint32_t start_time = pico_rtos_get_tick_count();
    
    result.processing_stage = 1;
    
    // Simulate complex processing algorithms
    result.processed_values[0] = sensor_data->temperature * 1.8f + 32.0f; // Convert to Fahrenheit
    result.processed_values[1] = sensor_data->humidity;
    result.processed_values[2] = sensor_data->pressure / 100.0f; // Convert to hPa
    result.processed_values[3] = (sensor_data->temperature + sensor_data->humidity) / 2.0f; // Comfort index
    
    // Calculate quality score based on data characteristics
    float temp_quality = (sensor_data->temperature > -10.0f && sensor_data->temperature < 50.0f) ? 1.0f : 0.5f;
    float humidity_quality = (sensor_data->humidity >= 0.0f && sensor_data->humidity <= 100.0f) ? 1.0f : 0.5f;
    float pressure_quality = (sensor_data->pressure > 900.0f && sensor_data->pressure < 1100.0f) ? 1.0f : 0.5f;
    
    result.quality_score = (uint32_t)((temp_quality + humidity_quality + pressure_quality) / 3.0f * 100.0f);
    
    // Simulate processing time
    pico_rtos_task_delay(5 + (sensor_data->sequence % 15));
    
    result.processing_time_us = (pico_rtos_get_tick_count() - start_time) * 1000;
    
    return result;
}

// =============================================================================
// CORE 0 TASKS (Sensor Data Acquisition and System Control)
// =============================================================================

/**
 * @brief Sensor data acquisition task (Core 0 affinity)
 */
void sensor_acquisition_task(void *param) {
    uint32_t sensor_id = (uint32_t)param;
    uint32_t sequence = 0;
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    printf("[Core %lu - Sensor %lu] Sensor acquisition task started\n", current_core, sensor_id);
    
    while (system_running) {
        // Generate sensor data
        sensor_data_t sensor_data = generate_sensor_data(sensor_id, sequence++);
        
        // Send sensor data to processing core (Core 1)
        bool sent = send_ipc_message_tracked(1, IPC_MSG_SENSOR_DATA, &sensor_data, 
                                           sizeof(sensor_data), 1000);
        
        if (sent) {
            if (sequence % 50 == 0) {
                printf("[Core %lu - Sensor %lu] Sent sensor data #%lu (T=%.1f°C, H=%.1f%%, P=%.1fhPa)\n",
                       current_core, sensor_id, sequence, sensor_data.temperature, 
                       sensor_data.humidity, sensor_data.pressure);
            }
        } else {
            printf("[Core %lu - Sensor %lu] Failed to send sensor data #%lu\n", 
                   current_core, sensor_id, sequence);
        }
        
        pico_rtos_task_delay(100 + (sensor_id * 50));  // Staggered sampling rates
    }
    
    printf("[Core %lu - Sensor %lu] Sensor acquisition task stopped\n", current_core, sensor_id);
}

/**
 * @brief System control task (Core 0 affinity)
 */
void system_control_task(void *param) {
    uint32_t control_sequence = 0;
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    printf("[Core %lu - Control] System control task started\n", current_core);
    
    while (system_running) {
        // Generate periodic control commands
        control_command_t command;
        command.command_id = 0x1000 + (control_sequence % 10);
        command.target_core = 1;  // Send commands to Core 1
        command.parameters[0] = control_sequence;
        command.parameters[1] = pico_rtos_get_tick_count();
        
        // Send control command to Core 1
        bool sent = send_ipc_message_tracked(1, IPC_MSG_CONTROL_CMD, &command, 
                                           sizeof(command), 500);
        
        if (sent) {
            printf("[Core %lu - Control] Sent control command 0x%04lX to Core 1\n",
                   current_core, command.command_id);
        } else {
            printf("[Core %lu - Control] Failed to send control command\n", current_core);
        }
        
        control_sequence++;
        
        // Send commands every 3 seconds
        pico_rtos_task_delay(3000);
    }
    
    printf("[Core %lu - Control] System control task stopped\n", current_core);
}

/**
 * @brief Core 0 heartbeat task
 */
void core0_heartbeat_task(void *param) {
    uint32_t heartbeat_sequence = 0;
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    printf("[Core %lu - Heartbeat] Heartbeat task started\n", current_core);
    
    while (system_running) {
        heartbeat_t heartbeat;
        heartbeat.core_id = current_core;
        heartbeat.timestamp = pico_rtos_get_tick_count();
        heartbeat.sequence = heartbeat_sequence++;
        
        // Send heartbeat to other core
        send_ipc_message_tracked(1, IPC_MSG_HEARTBEAT, &heartbeat, sizeof(heartbeat), 100);
        
        pico_rtos_task_delay(2000);  // Heartbeat every 2 seconds
    }
    
    printf("[Core %lu - Heartbeat] Heartbeat task stopped\n", current_core);
}

// =============================================================================
// CORE 1 TASKS (Data Processing and Communication)
// =============================================================================

/**
 * @brief Data processing task (Core 1 affinity)
 */
void data_processing_task(void *param) {
    uint32_t processor_id = (uint32_t)param;
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    printf("[Core %lu - Processor %lu] Data processing task started\n", current_core, processor_id);
    
    while (system_running) {
        pico_rtos_ipc_message_t message;
        sensor_data_t sensor_data;
        
        // Receive sensor data from Core 0
        bool received = pico_rtos_ipc_receive_message(&message, 2000);
        
        if (received && (message.type - PICO_RTOS_IPC_MSG_USER_DEFINED) == IPC_MSG_SENSOR_DATA) {
            // In a real implementation, you'd properly deserialize the data
            memcpy(&sensor_data, (void*)message.data1, sizeof(sensor_data));
            uint32_t start_time = pico_rtos_get_tick_count();
            
            // Process the sensor data
            processed_data_t processed = process_sensor_data(&sensor_data);
            
            uint32_t processing_time = (pico_rtos_get_tick_count() - start_time) * 1000;
            update_core_performance(current_core, processing_time, false, true);
            
            printf("[Core %lu - Processor %lu] Processed sensor data from sensor %lu (quality: %lu%%, time: %lu μs)\n",
                   current_core, processor_id, sensor_data.sensor_id, processed.quality_score, processing_time);
            
            // Send processed data back to Core 0 (or to another destination)
            if (processed.quality_score >= 80) {  // Only send high-quality data
                send_ipc_message_tracked(0, IPC_MSG_PROCESSED_DATA, &processed, 
                                       sizeof(processed), 500);
            }
            
        } else if (received && (message.type - PICO_RTOS_IPC_MSG_USER_DEFINED) == IPC_MSG_CONTROL_CMD) {
            control_command_t command;
            memcpy(&command, (void*)message.data1, sizeof(command));
            
            printf("[Core %lu - Processor %lu] Received control command 0x%04lX from Core %lu\n",
                   current_core, processor_id, command.command_id, message.source_core);
            
            // Process control command
            switch (command.command_id & 0xFF) {
                case 0x00:
                    printf("[Core %lu - Processor %lu] Executing system reset command\n", current_core, processor_id);
                    break;
                case 0x01:
                    printf("[Core %lu - Processor %lu] Executing configuration update command\n", current_core, processor_id);
                    break;
                case 0x02:
                    printf("[Core %lu - Processor %lu] Executing diagnostic command\n", current_core, processor_id);
                    break;
                default:
                    printf("[Core %lu - Processor %lu] Executing generic command\n", current_core, processor_id);
                    break;
            }
            
        } else if (!received) {
            // Timeout - normal condition
            printf("[Core %lu - Processor %lu] No messages received, continuing...\n", current_core, processor_id);
        }
    }
    
    printf("[Core %lu - Processor %lu] Data processing task stopped\n", current_core, processor_id);
}

/**
 * @brief Core 1 status reporter task
 */
void core1_status_task(void *param) {
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    printf("[Core %lu - Status] Status reporter task started\n", current_core);
    
    while (system_running) {
        // Collect core statistics
        pico_rtos_core_state_t *core_state = pico_rtos_smp_get_core_state(current_core);
        if (core_state) {
            status_update_t status;
            status.core_id = current_core;
            status.cpu_usage_percent = core_state->load_percentage;
            status.active_tasks = core_state->task_count;
            status.uptime_ms = pico_rtos_get_uptime_ms();
            
            // Get performance statistics
            if (pico_rtos_mutex_lock(&performance_mutex, 100)) {
                status.messages_processed = core1_performance.messages_received;
                pico_rtos_mutex_unlock(&performance_mutex);
            }
            
            // Send status update to Core 0
            send_ipc_message_tracked(0, IPC_MSG_STATUS_UPDATE, &status, sizeof(status), 500);
            
            printf("[Core %lu - Status] CPU: %lu%%, Tasks: %lu, Messages: %lu\n",
                   current_core, status.cpu_usage_percent, status.active_tasks, status.messages_processed);
        }
        
        pico_rtos_task_delay(5000);  // Status update every 5 seconds
    }
    
    printf("[Core %lu - Status] Status reporter task stopped\n", current_core);
}

// =============================================================================
// SHARED TASKS (Can run on any core with load balancing)
// =============================================================================

/**
 * @brief Flexible worker task (can run on any core)
 */
void flexible_worker_task(void *param) {
    uint32_t worker_id = (uint32_t)param;
    uint32_t work_cycles = 0;
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    printf("[Core %lu - Worker %lu] Flexible worker task started\n", current_core, worker_id);
    
    while (system_running) {
        current_core = pico_rtos_smp_get_core_id();  // Core may change due to load balancing
        
        // Simulate variable workload
        uint32_t work_intensity = 10 + (work_cycles % 50);  // Variable work intensity
        
        printf("[Core %lu - Worker %lu] Performing work cycle %lu (intensity: %lu)\n",
               current_core, worker_id, work_cycles, work_intensity);
        
        // Simulate computational work
        float result = 0.0f;
        for (uint32_t i = 0; i < work_intensity * 100; i++) {
            result += sinf((float)i / 100.0f) * cosf((float)work_cycles / 50.0f);
        }
        
        work_cycles++;
        
        // Variable delay to create different load patterns
        uint32_t delay = 200 + (work_cycles % 800);
        pico_rtos_task_delay(delay);
    }
    
    printf("[Core %lu - Worker %lu] Flexible worker task stopped (completed %lu cycles)\n", 
           current_core, worker_id, work_cycles);
}

// =============================================================================
// MONITORING AND CONTROL TASKS
// =============================================================================

/**
 * @brief System monitor task (monitors both cores)
 */
void system_monitor_task(void *param) {
    uint32_t monitor_cycle = 0;
    uint32_t current_core = pico_rtos_smp_get_core_id();
    
    printf("[Core %lu - Monitor] System monitor started\n", current_core);
    
    while (system_running) {
        monitor_cycle++;
        
        printf("\n=== MULTI-CORE SYSTEM REPORT #%lu ===\n", monitor_cycle);
        printf("System uptime: %lu ms\n", pico_rtos_get_uptime_ms());
        
        // Get statistics for both cores
        pico_rtos_core_state_t *core0_state = pico_rtos_smp_get_core_state(0);
        pico_rtos_core_state_t *core1_state = pico_rtos_smp_get_core_state(1);
        
        if (core0_state && core1_state) {
            printf("\nCore Statistics:\n");
            printf("  Core 0: %lu%% CPU, %lu tasks, %lu context switches\n",
                   core0_state->load_percentage, core0_state->task_count, core0_state->context_switches);
            printf("  Core 1: %lu%% CPU, %lu tasks, %lu context switches\n",
                   core1_state->load_percentage, core1_state->task_count, core1_state->context_switches);
            
            // Check for load imbalance
            int32_t load_imbalance = (int32_t)core0_state->load_percentage - (int32_t)core1_state->load_percentage;
            printf("  Load imbalance: %ld%% ", load_imbalance);
            if (abs(load_imbalance) > 20) {
                printf("(HIGH - consider rebalancing)");
            } else {
                printf("(acceptable)");
            }
            printf("\n");
        }
        
        // Get performance statistics
        core_performance_t core0_perf, core1_perf;
        if (pico_rtos_mutex_lock(&performance_mutex, 1000)) {
            core0_perf = core0_performance;
            core1_perf = core1_performance;
            pico_rtos_mutex_unlock(&performance_mutex);
        }
        
        printf("\nIPC Performance:\n");
        printf("  Core 0: %lu sent, %lu received, %lu failures\n",
               core0_perf.messages_sent, core0_perf.messages_received, core0_perf.ipc_failures);
        printf("  Core 1: %lu sent, %lu received, %lu failures\n",
               core1_perf.messages_sent, core1_perf.messages_received, core1_perf.ipc_failures);
        
        if (core1_perf.processing_cycles > 0) {
            printf("  Core 1 Processing: avg %lu μs, min %lu μs, max %lu μs\n",
                   core1_perf.total_processing_time_us / core1_perf.processing_cycles,
                   core1_perf.min_processing_time_us, core1_perf.max_processing_time_us);
        }
        
        printf("========================================\n\n");
        
        // Trigger shutdown after extended operation
        if (monitor_cycle >= 15) {
            printf("[Core %lu - Monitor] Demonstration complete, triggering shutdown\n", current_core);
            system_running = false;
        }
        
        pico_rtos_task_delay(6000);  // Report every 6 seconds
    }
    
    printf("[Core %lu - Monitor] System monitor stopped\n", current_core);
}

/**
 * @brief LED status indicator
 */
void led_status_task(void *param) {
    bool led_state = false;
    uint32_t blink_rate = 500;
    
    while (system_running) {
        // Get current core load to adjust blink rate
        pico_rtos_core_state_t *core0_state = pico_rtos_smp_get_core_state(0);
        pico_rtos_core_state_t *core1_state = pico_rtos_smp_get_core_state(1);
        if (core0_state && core1_state) {
            
            uint32_t avg_load = (core0_state->load_percentage + core1_state->load_percentage) / 2;
            
            if (avg_load > 80) {
                blink_rate = 100;  // Very fast blink for high load
            } else if (avg_load > 50) {
                blink_rate = 250;  // Fast blink for medium load
            } else {
                blink_rate = 500;  // Normal blink for low load
            }
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
    
    printf("\n=== Pico-RTOS v0.3.1 Multi-Core Load Balancing and IPC Example ===\n");
    printf("This example demonstrates:\n");
    printf("1. Task distribution across both RP2040 cores\n");
    printf("2. Core affinity assignment and load balancing\n");
    printf("3. Inter-core communication using IPC messages\n");
    printf("4. Core-specific resource management\n");
    printf("5. Performance monitoring across cores\n\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize multi-core support
    printf("Initializing multi-core support...\n");
    if (!pico_rtos_smp_init()) {
        printf("ERROR: Failed to initialize SMP system\n");
        return -1;
    }
    
    // Enable load balancing
    pico_rtos_smp_set_load_balancing(true);
    pico_rtos_smp_set_load_balance_threshold(20);  // 20% imbalance threshold
    
    // Initialize synchronization primitives
    if (!pico_rtos_mutex_init(&performance_mutex)) {
        printf("ERROR: Failed to initialize performance mutex\n");
        return -1;
    }
    
    printf("Multi-core system initialized successfully\n\n");
    
    // Create Core 0 specific tasks (sensor acquisition and control)
    printf("Creating Core 0 specific tasks...\n");
    static pico_rtos_task_t sensor1_task, sensor2_task, control_task, heartbeat0_task;
    
    if (!pico_rtos_task_create(&sensor1_task, "Sensor1", sensor_acquisition_task, (void*)1, 1024, 5) ||
        !pico_rtos_task_create(&sensor2_task, "Sensor2", sensor_acquisition_task, (void*)2, 1024, 5) ||
        !pico_rtos_task_create(&control_task, "Control", system_control_task, NULL, 1024, 4) ||
        !pico_rtos_task_create(&heartbeat0_task, "Heartbeat0", core0_heartbeat_task, NULL, 512, 2)) {
        printf("ERROR: Failed to create Core 0 tasks\n");
        return -1;
    }
    
    // Set Core 0 affinity for these tasks
    pico_rtos_smp_set_task_affinity(&sensor1_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    pico_rtos_smp_set_task_affinity(&sensor2_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    pico_rtos_smp_set_task_affinity(&control_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    pico_rtos_smp_set_task_affinity(&heartbeat0_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    
    // Create Core 1 specific tasks (data processing and communication)
    printf("Creating Core 1 specific tasks...\n");
    static pico_rtos_task_t processor1_task, processor2_task, status1_task;
    
    if (!pico_rtos_task_create(&processor1_task, "Processor1", data_processing_task, (void*)1, 1024, 4) ||
        !pico_rtos_task_create(&processor2_task, "Processor2", data_processing_task, (void*)2, 1024, 4) ||
        !pico_rtos_task_create(&status1_task, "Status1", core1_status_task, NULL, 1024, 2)) {
        printf("ERROR: Failed to create Core 1 tasks\n");
        return -1;
    }
    
    // Set Core 1 affinity for these tasks
    pico_rtos_smp_set_task_affinity(&processor1_task, PICO_RTOS_CORE_AFFINITY_CORE1);
    pico_rtos_smp_set_task_affinity(&processor2_task, PICO_RTOS_CORE_AFFINITY_CORE1);
    pico_rtos_smp_set_task_affinity(&status1_task, PICO_RTOS_CORE_AFFINITY_CORE1);
    
    // Create flexible tasks that can run on any core (load balanced)
    printf("Creating flexible load-balanced tasks...\n");
    static pico_rtos_task_t worker1_task, worker2_task, worker3_task;
    
    if (!pico_rtos_task_create(&worker1_task, "Worker1", flexible_worker_task, (void*)1, 1024, 3) ||
        !pico_rtos_task_create(&worker2_task, "Worker2", flexible_worker_task, (void*)2, 1024, 3) ||
        !pico_rtos_task_create(&worker3_task, "Worker3", flexible_worker_task, (void*)3, 1024, 3)) {
        printf("ERROR: Failed to create flexible worker tasks\n");
        return -1;
    }
    
    // Set flexible affinity for load balancing
    pico_rtos_smp_set_task_affinity(&worker1_task, PICO_RTOS_CORE_AFFINITY_ANY);
    pico_rtos_smp_set_task_affinity(&worker2_task, PICO_RTOS_CORE_AFFINITY_ANY);
    pico_rtos_smp_set_task_affinity(&worker3_task, PICO_RTOS_CORE_AFFINITY_ANY);
    
    // Create monitoring tasks
    printf("Creating monitoring tasks...\n");
    static pico_rtos_task_t monitor_task, led_task;
    
    if (!pico_rtos_task_create(&monitor_task, "Monitor", system_monitor_task, NULL, 1024, 1) ||
        !pico_rtos_task_create(&led_task, "LED", led_status_task, NULL, 256, 1)) {
        printf("ERROR: Failed to create monitoring tasks\n");
        return -1;
    }
    
    // Monitor task can run on any core
    pico_rtos_smp_set_task_affinity(&monitor_task, PICO_RTOS_CORE_AFFINITY_ANY);
    pico_rtos_smp_set_task_affinity(&led_task, PICO_RTOS_CORE_AFFINITY_ANY);
    
    printf("All tasks created successfully\n");
    printf("Load balancing enabled with 20%% threshold\n");
    printf("Starting scheduler...\n\n");
    
    // Start the RTOS scheduler
    pico_rtos_start();
    
    // Should never reach here
    printf("ERROR: Scheduler returned unexpectedly\n");
    return -1;
}