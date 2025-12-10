/**
 * @file main.c
 * @brief Stream Buffer Efficient Data Streaming Example for Pico-RTOS v0.3.1
 * 
 * This example demonstrates efficient data streaming using stream buffers with:
 * 
 * 1. Variable-length message streaming between tasks
 * 2. High-throughput data pipeline with multiple producers and consumers
 * 3. Zero-copy optimization for large data transfers
 * 4. Flow control and backpressure handling
 * 5. Performance monitoring and throughput analysis
 * 6. Different message types and priorities
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"

// Disable migration warnings for examples (they already use v0.3.1 APIs)
#define PICO_RTOS_DISABLE_MIGRATION_WARNINGS
#include "pico_rtos.h"

// =============================================================================
// CONFIGURATION AND MESSAGE DEFINITIONS
// =============================================================================

#define LED_PIN 25

// Stream buffer sizes for different data types
#define SENSOR_STREAM_SIZE      2048
#define PROCESSED_STREAM_SIZE   4096
#define COMMAND_STREAM_SIZE     512
#define LOG_STREAM_SIZE         1024

// Message types
#define MSG_TYPE_SENSOR_DATA    1
#define MSG_TYPE_PROCESSED_DATA 2
#define MSG_TYPE_COMMAND        3
#define MSG_TYPE_LOG_ENTRY      4

// Message priorities
#define MSG_PRIORITY_LOW        1
#define MSG_PRIORITY_NORMAL     2
#define MSG_PRIORITY_HIGH       3
#define MSG_PRIORITY_CRITICAL   4

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// Common message header
typedef struct {
    uint32_t type;
    uint32_t priority;
    uint32_t timestamp;
    uint32_t source_id;
    uint32_t sequence;
    uint32_t data_length;
} message_header_t;

// Sensor data message
typedef struct {
    message_header_t header;
    uint32_t sensor_id;
    float temperature;
    float humidity;
    float pressure;
    float light_level;
    uint32_t sample_count;
    uint8_t status_flags;
} sensor_data_msg_t;

// Performance statistics
typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t send_failures;
    uint32_t receive_timeouts;
    uint32_t total_latency_us;
    uint32_t max_latency_us;
    uint32_t min_latency_us;
} stream_stats_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Stream buffers for different data flows
static pico_rtos_stream_buffer_t sensor_stream;
static pico_rtos_stream_buffer_t processed_stream;

// Buffer storage
static uint8_t sensor_stream_buffer[SENSOR_STREAM_SIZE];
static uint8_t processed_stream_buffer[PROCESSED_STREAM_SIZE];

// Performance statistics
static stream_stats_t sensor_stream_stats = {0};
static stream_stats_t processed_stream_stats = {0};

// Synchronization
static pico_rtos_mutex_t stats_mutex;

// System control
static volatile bool system_running = true;

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Update stream statistics in a thread-safe manner
 */
void update_stream_stats(stream_stats_t *stats, bool is_send, uint32_t bytes, uint32_t latency_us, bool success) {
    if (pico_rtos_mutex_lock(&stats_mutex, 100)) {
        if (is_send) {
            if (success) {
                stats->messages_sent++;
                stats->bytes_sent += bytes;
            } else {
                stats->send_failures++;
            }
        } else {
            if (success) {
                stats->messages_received++;
                stats->bytes_received += bytes;
                stats->total_latency_us += latency_us;
                if (latency_us > stats->max_latency_us) {
                    stats->max_latency_us = latency_us;
                }
                if (stats->min_latency_us == 0 || latency_us < stats->min_latency_us) {
                    stats->min_latency_us = latency_us;
                }
            } else {
                stats->receive_timeouts++;
            }
        }
        pico_rtos_mutex_unlock(&stats_mutex);
    }
}

/**
 * @brief Send a message to a stream buffer with statistics tracking
 */
bool send_message_with_stats(pico_rtos_stream_buffer_t *stream, const void *message, 
                            uint32_t length, uint32_t timeout, stream_stats_t *stats) {
    uint32_t start_time = pico_rtos_get_tick_count();
    
    uint32_t bytes_sent = pico_rtos_stream_buffer_send(stream, message, length, timeout);
    bool success = (bytes_sent == length);
    
    uint32_t latency = (pico_rtos_get_tick_count() - start_time) * 1000; // Convert to microseconds
    update_stream_stats(stats, true, bytes_sent, latency, success);
    
    return success;
}

/**
 * @brief Receive a message from a stream buffer with statistics tracking
 */
uint32_t receive_message_with_stats(pico_rtos_stream_buffer_t *stream, void *buffer, 
                                   uint32_t max_length, uint32_t timeout, stream_stats_t *stats) {
    uint32_t start_time = pico_rtos_get_tick_count();
    
    uint32_t bytes_received = pico_rtos_stream_buffer_receive(stream, buffer, max_length, timeout);
    bool success = (bytes_received > 0);
    
    uint32_t latency = (pico_rtos_get_tick_count() - start_time) * 1000; // Convert to microseconds
    update_stream_stats(stats, false, bytes_received, latency, success);
    
    return bytes_received;
}

/**
 * @brief Initialize a message header
 */
void init_message_header(message_header_t *header, uint32_t type, uint32_t priority, 
                        uint32_t source_id, uint32_t sequence, uint32_t data_length) {
    header->type = type;
    header->priority = priority;
    header->timestamp = pico_rtos_get_tick_count();
    header->source_id = source_id;
    header->sequence = sequence;
    header->data_length = data_length;
}

/**
 * @brief Calculate message latency from header timestamp
 */
uint32_t calculate_message_latency(const message_header_t *header) {
    return (pico_rtos_get_tick_count() - header->timestamp) * 1000; // Convert to microseconds
}

/**
 * @brief Simulate sensor reading with some variability
 */
sensor_data_msg_t simulate_sensor_reading(uint32_t sensor_id, uint32_t sequence) {
    sensor_data_msg_t data;
    uint32_t tick = pico_rtos_get_tick_count();
    
    init_message_header(&data.header, MSG_TYPE_SENSOR_DATA, MSG_PRIORITY_NORMAL, 
                       sensor_id, sequence, sizeof(data) - sizeof(message_header_t));
    
    data.sensor_id = sensor_id;
    data.sample_count = sequence;
    
    // Simulate sensor readings with some variation
    data.temperature = 20.0f + (float)(tick % 100) / 10.0f + sinf((float)tick / 1000.0f) * 5.0f;
    data.humidity = 45.0f + (float)(tick % 200) / 20.0f + cosf((float)tick / 1500.0f) * 10.0f;
    data.pressure = 1013.25f + sinf((float)tick / 2000.0f) * 20.0f;
    data.light_level = 500.0f + (float)(sequence % 1000);
    
    // Occasionally simulate invalid readings
    data.status_flags = (tick % 47) != 0 ? 0x01 : 0x00;  // ~2% invalid readings
    
    return data;
}

// =============================================================================
// SENSOR DATA PRODUCER TASKS
// =============================================================================

/**
 * @brief High-frequency sensor data producer
 */
void high_freq_sensor_task(void *param) {
    uint32_t sensor_id = (uint32_t)param;
    uint32_t sequence = 0;
    
    printf("[HF Sensor %lu] High-frequency sensor task started\n", sensor_id);
    
    while (system_running) {
        // Generate sensor data
        sensor_data_msg_t sensor_msg = simulate_sensor_reading(sensor_id, sequence++);
        
        // Try to send with short timeout to avoid blocking
        if (send_message_with_stats(&sensor_stream, &sensor_msg, sizeof(sensor_msg), 
                                   100, &sensor_stream_stats)) {
            if (sequence % 100 == 0) {
                printf("[HF Sensor %lu] Sent sample #%lu (T=%.1f°C, H=%.1f%%, P=%.1fhPa)\n",
                       sensor_id, sequence, sensor_msg.temperature, sensor_msg.humidity, sensor_msg.pressure);
            }
        } else {
            printf("[HF Sensor %lu] Stream buffer full, sample #%lu dropped\n", sensor_id, sequence);
        }
        
        pico_rtos_task_delay(50);  // 20 Hz sampling rate
    }
    
    printf("[HF Sensor %lu] High-frequency sensor task stopped\n", sensor_id);
}

/**
 * @brief Low-frequency sensor data producer with larger messages
 */
void low_freq_sensor_task(void *param) {
    uint32_t sensor_id = (uint32_t)param;
    uint32_t sequence = 0;
    
    printf("[LF Sensor %lu] Low-frequency sensor task started\n", sensor_id);
    
    while (system_running) {
        // Create a batch of sensor readings
        for (int batch = 0; batch < 5; batch++) {
            sensor_data_msg_t sensor_msg = simulate_sensor_reading(sensor_id, sequence++);
            
            // Mark end of batch
            if (batch == 4) {
                sensor_msg.header.priority = MSG_PRIORITY_HIGH;
                sensor_msg.status_flags |= 0x02; // End of batch flag
            }
            
            // Send with longer timeout for batch processing
            if (!send_message_with_stats(&sensor_stream, &sensor_msg, sizeof(sensor_msg), 
                                        1000, &sensor_stream_stats)) {
                printf("[LF Sensor %lu] Failed to send batch item %d, sequence #%lu\n", 
                       sensor_id, batch, sequence);
            }
        }
        
        printf("[LF Sensor %lu] Sent batch ending with sequence #%lu\n", sensor_id, sequence - 1);
        
        pico_rtos_task_delay(2000);  // 0.5 Hz batch rate
    }
    
    printf("[LF Sensor %lu] Low-frequency sensor task stopped\n", sensor_id);
}

// =============================================================================
// DATA PROCESSING TASKS
// =============================================================================

/**
 * @brief Primary data processor
 */
void primary_processor_task(void *param) {
    uint32_t processor_id = (uint32_t)param;
    uint32_t processed_count = 0;
    uint8_t receive_buffer[512];
    
    printf("[Processor %lu] Primary data processor started\n", processor_id);
    
    while (system_running) {
        // Receive sensor data
        uint32_t bytes_received = receive_message_with_stats(&sensor_stream, receive_buffer, 
                                                            sizeof(receive_buffer), 2000, &sensor_stream_stats);
        
        if (bytes_received >= sizeof(message_header_t)) {
            message_header_t *header = (message_header_t *)receive_buffer;
            
            // Check message type
            if (header->type == MSG_TYPE_SENSOR_DATA && bytes_received >= sizeof(sensor_data_msg_t)) {
                sensor_data_msg_t *sensor_msg = (sensor_data_msg_t *)receive_buffer;
                uint32_t latency = calculate_message_latency(header);
                
                printf("[Processor %lu] Processing sensor data from sensor %lu (seq: %lu, latency: %lu μs)\n",
                       processor_id, sensor_msg->sensor_id, header->sequence, latency);
                
                // Simulate data processing
                pico_rtos_task_delay(10 + (processed_count % 20));
                processed_count++;
                
            } else {
                printf("[Processor %lu] Received unexpected message type: %lu\n", processor_id, header->type);
            }
        } else if (bytes_received == 0) {
            // Timeout - normal condition
            printf("[Processor %lu] No data received, continuing...\n", processor_id);
        }
    }
    
    printf("[Processor %lu] Primary data processor stopped\n", processor_id);
}

// =============================================================================
// MONITORING TASKS
// =============================================================================

/**
 * @brief Performance monitor task
 */
void performance_monitor_task(void *param) {
    uint32_t report_counter = 0;
    
    printf("[Performance Monitor] Performance monitoring started\n");
    
    while (system_running) {
        report_counter++;
        
        printf("\n=== STREAM BUFFER PERFORMANCE REPORT #%lu ===\n", report_counter);
        printf("System uptime: %lu ms\n", pico_rtos_get_uptime_ms());
        
        // Get statistics snapshots
        stream_stats_t sensor_stats, processed_stats;
        
        if (pico_rtos_mutex_lock(&stats_mutex, 1000)) {
            sensor_stats = sensor_stream_stats;
            processed_stats = processed_stream_stats;
            pico_rtos_mutex_unlock(&stats_mutex);
        }
        
        // Sensor stream statistics
        printf("\nSensor Stream:\n");
        printf("  Messages: %lu sent, %lu received\n", sensor_stats.messages_sent, sensor_stats.messages_received);
        printf("  Bytes: %lu sent, %lu received\n", sensor_stats.bytes_sent, sensor_stats.bytes_received);
        printf("  Failures: %lu send failures, %lu receive timeouts\n", sensor_stats.send_failures, sensor_stats.receive_timeouts);
        if (sensor_stats.messages_received > 0) {
            printf("  Latency: avg %lu μs, min %lu μs, max %lu μs\n",
                   sensor_stats.total_latency_us / sensor_stats.messages_received,
                   sensor_stats.min_latency_us, sensor_stats.max_latency_us);
        }
        printf("  Buffer utilization: %lu/%lu bytes (%.1f%%)\n",
               pico_rtos_stream_buffer_bytes_available(&sensor_stream), SENSOR_STREAM_SIZE,
               (float)pico_rtos_stream_buffer_bytes_available(&sensor_stream) / SENSOR_STREAM_SIZE * 100.0f);
        
        // Calculate overall throughput
        uint32_t total_bytes = sensor_stats.bytes_sent + processed_stats.bytes_sent;
        uint32_t uptime_seconds = pico_rtos_get_uptime_ms() / 1000;
        if (uptime_seconds > 0) {
            printf("\nOverall Throughput: %lu bytes/sec\n", total_bytes / uptime_seconds);
        }
        
        printf("=============================================\n\n");
        
        // Trigger shutdown after extended operation for demonstration
        if (report_counter >= 10) {
            printf("[Performance Monitor] Demonstration complete, triggering shutdown\n");
            system_running = false;
        }
        
        pico_rtos_task_delay(8000);  // Report every 8 seconds
    }
    
    printf("[Performance Monitor] Performance monitoring stopped\n");
}

/**
 * @brief LED status indicator
 */
void led_status_task(void *param) {
    bool led_state = false;
    uint32_t blink_rate = 500;
    
    while (system_running) {
        // Adjust blink rate based on system activity
        uint32_t sensor_buffer_usage = pico_rtos_stream_buffer_bytes_available(&sensor_stream);
        if (sensor_buffer_usage > SENSOR_STREAM_SIZE * 0.8f) {
            blink_rate = 100;  // Fast blink when buffer nearly full
        } else if (sensor_buffer_usage > SENSOR_STREAM_SIZE * 0.5f) {
            blink_rate = 250;  // Medium blink when buffer half full
        } else {
            blink_rate = 500;  // Slow blink when buffer has space
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
    
    printf("\n=== Pico-RTOS v0.3.1 Stream Buffer Efficient Data Streaming Example ===\n");
    printf("This example demonstrates:\n");
    printf("1. Variable-length message streaming between tasks\n");
    printf("2. High-throughput data pipeline with multiple producers and consumers\n");
    printf("3. Flow control and backpressure handling\n");
    printf("4. Performance monitoring and throughput analysis\n\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize stream buffers
    printf("Initializing stream buffers...\n");
    if (!pico_rtos_stream_buffer_init(&sensor_stream, sensor_stream_buffer, SENSOR_STREAM_SIZE) ||
        !pico_rtos_stream_buffer_init(&processed_stream, processed_stream_buffer, PROCESSED_STREAM_SIZE)) {
        printf("ERROR: Failed to initialize stream buffers\n");
        return -1;
    }
    
    // Initialize synchronization primitives
    if (!pico_rtos_mutex_init(&stats_mutex)) {
        printf("ERROR: Failed to initialize statistics mutex\n");
        return -1;
    }
    
    printf("Stream buffers initialized successfully\n\n");
    
    // Create sensor producer tasks
    printf("Creating sensor producer tasks...\n");
    static pico_rtos_task_t hf_sensor1_task, hf_sensor2_task, lf_sensor_task;
    
    if (!pico_rtos_task_create(&hf_sensor1_task, "HFSensor1", high_freq_sensor_task, (void*)1, 1024, 4) ||
        !pico_rtos_task_create(&hf_sensor2_task, "HFSensor2", high_freq_sensor_task, (void*)2, 1024, 4) ||
        !pico_rtos_task_create(&lf_sensor_task, "LFSensor", low_freq_sensor_task, (void*)3, 1024, 3)) {
        printf("ERROR: Failed to create sensor producer tasks\n");
        return -1;
    }
    
    // Create data processing tasks
    printf("Creating data processing tasks...\n");
    static pico_rtos_task_t primary_proc_task;
    
    if (!pico_rtos_task_create(&primary_proc_task, "PrimaryProc", primary_processor_task, (void*)1, 1024, 3)) {
        printf("ERROR: Failed to create data processing tasks\n");
        return -1;
    }
    
    // Create monitoring tasks
    printf("Creating monitoring tasks...\n");
    static pico_rtos_task_t perf_monitor_task, led_task;
    
    if (!pico_rtos_task_create(&perf_monitor_task, "PerfMonitor", performance_monitor_task, NULL, 1024, 1) ||
        !pico_rtos_task_create(&led_task, "LEDStatus", led_status_task, NULL, 256, 1)) {
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