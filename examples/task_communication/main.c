/**
 * @file main.c
 * @brief Multi-Task Communication Patterns Example for Pico-RTOS
 * 
 * This example demonstrates three key inter-task communication patterns:
 * 1. Queue-based producer-consumer pattern
 * 2. Semaphore-based resource sharing
 * 3. Task notification lightweight messaging (using queues for notifications)
 * 
 * Each pattern includes proper error handling and timeout management.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

// =============================================================================
// CONFIGURATION AND DATA STRUCTURES
// =============================================================================

#define LED_PIN 25
#define QUEUE_SIZE 10
#define SHARED_RESOURCE_COUNT 3
#define NOTIFICATION_QUEUE_SIZE 5

// Data structure for queue-based communication
typedef struct {
    uint32_t id;
    uint32_t timestamp;
    char message[32];
} data_packet_t;

// Data structure for notifications
typedef struct {
    uint32_t sender_id;
    uint32_t notification_type;
    uint32_t data;
} notification_t;

// Notification types
#define NOTIFICATION_WORK_COMPLETE 1
#define NOTIFICATION_ERROR_OCCURRED 2
#define NOTIFICATION_STATUS_UPDATE 3

// =============================================================================
// GLOBAL COMMUNICATION OBJECTS
// =============================================================================

// Queue-based communication
static pico_rtos_queue_t data_queue;
static data_packet_t queue_buffer[QUEUE_SIZE];

// Semaphore-based resource sharing
static pico_rtos_semaphore_t resource_semaphore;
static pico_rtos_mutex_t shared_resource_mutex;
static uint32_t shared_resource_value = 0;
static uint32_t resource_access_counter = 0;

// Task notification system (using queues)
static pico_rtos_queue_t notification_queue;
static notification_t notification_buffer[NOTIFICATION_QUEUE_SIZE];

// Statistics tracking
static uint32_t packets_produced = 0;
static uint32_t packets_consumed = 0;
static uint32_t resource_accesses = 0;
static uint32_t notifications_sent = 0;
static uint32_t notifications_received = 0;

// =============================================================================
// QUEUE-BASED PRODUCER-CONSUMER PATTERN
// =============================================================================

/**
 * @brief Producer task - generates data packets and sends them via queue
 * 
 * This task demonstrates:
 * - Queue sending with timeout handling
 * - Error handling for queue full conditions
 * - Proper data packet construction
 */
void producer_task(void *param) {
    uint32_t producer_id = (uint32_t)param;
    data_packet_t packet;
    uint32_t sequence = 0;
    
    printf("[Producer %lu] Starting producer task\n", producer_id);
    
    while (1) {
        // Prepare data packet
        packet.id = producer_id;
        packet.timestamp = pico_rtos_get_tick_count();
        snprintf(packet.message, sizeof(packet.message), "Data_%lu_%lu", producer_id, sequence++);
        
        // Try to send packet with timeout
        if (pico_rtos_queue_send(&data_queue, &packet, 1000)) {
            packets_produced++;
            printf("[Producer %lu] Sent: %s (total: %lu)\n", 
                   producer_id, packet.message, packets_produced);
        } else {
            printf("[Producer %lu] ERROR: Queue send timeout - queue may be full\n", producer_id);
        }
        
        // Vary production rate based on producer ID
        pico_rtos_task_delay(500 + (producer_id * 200));
    }
}

/**
 * @brief Consumer task - receives and processes data packets from queue
 * 
 * This task demonstrates:
 * - Queue receiving with timeout handling
 * - Data validation and processing
 * - Error handling for queue empty conditions
 */
void consumer_task(void *param) {
    uint32_t consumer_id = (uint32_t)param;
    data_packet_t packet;
    
    printf("[Consumer %lu] Starting consumer task\n", consumer_id);
    
    while (1) {
        // Try to receive packet with timeout
        if (pico_rtos_queue_receive(&data_queue, &packet, 2000)) {
            packets_consumed++;
            
            // Process the received data
            printf("[Consumer %lu] Received: %s from Producer %lu (age: %lu ms, total: %lu)\n",
                   consumer_id, packet.message, packet.id,
                   pico_rtos_get_tick_count() - packet.timestamp, packets_consumed);
            
            // Simulate processing time
            pico_rtos_task_delay(100);
            
        } else {
            printf("[Consumer %lu] No data received within timeout period\n", consumer_id);
        }
    }
}

// =============================================================================
// SEMAPHORE-BASED RESOURCE SHARING
// =============================================================================

/**
 * @brief Resource worker task - demonstrates semaphore-based resource access
 * 
 * This task demonstrates:
 * - Semaphore acquisition with timeout
 * - Mutex protection for shared data
 * - Proper resource cleanup and release
 * - Error handling for resource contention
 */
void resource_worker_task(void *param) {
    uint32_t worker_id = (uint32_t)param;
    uint32_t work_cycles = 0;
    
    printf("[Worker %lu] Starting resource worker task\n", worker_id);
    
    while (1) {
        // Try to acquire a resource slot (semaphore)
        if (pico_rtos_semaphore_take(&resource_semaphore, 3000)) {
            printf("[Worker %lu] Acquired resource slot\n", worker_id);
            
            // Access shared resource with mutex protection
            if (pico_rtos_mutex_lock(&shared_resource_mutex, 1000)) {
                // Critical section - modify shared resource
                uint32_t old_value = shared_resource_value;
                shared_resource_value += (worker_id * 10);
                resource_access_counter++;
                resource_accesses++;
                
                printf("[Worker %lu] Modified shared resource: %lu -> %lu (access #%lu)\n",
                       worker_id, old_value, shared_resource_value, resource_access_counter);
                
                // Simulate work on the resource
                pico_rtos_task_delay(200 + (worker_id * 50));
                
                // Release mutex
                pico_rtos_mutex_unlock(&shared_resource_mutex);
            } else {
                printf("[Worker %lu] ERROR: Failed to acquire resource mutex\n", worker_id);
            }
            
            // Release the resource slot (semaphore)
            if (pico_rtos_semaphore_give(&resource_semaphore)) {
                printf("[Worker %lu] Released resource slot\n", worker_id);
            } else {
                printf("[Worker %lu] ERROR: Failed to release semaphore\n", worker_id);
            }
            
            work_cycles++;
        } else {
            printf("[Worker %lu] ERROR: Resource acquisition timeout (no slots available)\n", worker_id);
        }
        
        // Wait before next resource access attempt
        pico_rtos_task_delay(1000 + (worker_id * 300));
    }
}

// =============================================================================
// TASK NOTIFICATION LIGHTWEIGHT MESSAGING
// =============================================================================

/**
 * @brief Notification sender task - sends lightweight notifications
 * 
 * This task demonstrates:
 * - Lightweight notification sending using queues
 * - Different notification types
 * - Non-blocking notification attempts
 */
void notification_sender_task(void *param) {
    uint32_t sender_id = (uint32_t)param;
    notification_t notification;
    uint32_t cycle = 0;
    
    printf("[Sender %lu] Starting notification sender task\n", sender_id);
    
    while (1) {
        // Prepare notification based on cycle
        notification.sender_id = sender_id;
        notification.data = cycle;
        
        // Cycle through different notification types
        switch (cycle % 3) {
            case 0:
                notification.notification_type = NOTIFICATION_WORK_COMPLETE;
                break;
            case 1:
                notification.notification_type = NOTIFICATION_STATUS_UPDATE;
                break;
            case 2:
                notification.notification_type = NOTIFICATION_ERROR_OCCURRED;
                break;
        }
        
        // Try to send notification (non-blocking)
        if (pico_rtos_queue_send(&notification_queue, &notification, PICO_RTOS_NO_WAIT)) {
            notifications_sent++;
            printf("[Sender %lu] Sent notification type %lu, data: %lu (total: %lu)\n",
                   sender_id, notification.notification_type, notification.data, notifications_sent);
        } else {
            printf("[Sender %lu] WARNING: Notification queue full, notification dropped\n", sender_id);
        }
        
        cycle++;
        pico_rtos_task_delay(800 + (sender_id * 400));
    }
}

/**
 * @brief Notification receiver task - processes incoming notifications
 * 
 * This task demonstrates:
 * - Notification receiving with timeout
 * - Different handling based on notification type
 * - Statistics tracking
 */
void notification_receiver_task(void *param) {
    notification_t notification;
    
    printf("[Receiver] Starting notification receiver task\n");
    
    while (1) {
        // Wait for notifications with timeout
        if (pico_rtos_queue_receive(&notification_queue, &notification, 5000)) {
            notifications_received++;
            
            // Handle different notification types
            switch (notification.notification_type) {
                case NOTIFICATION_WORK_COMPLETE:
                    printf("[Receiver] Work complete notification from Sender %lu (data: %lu)\n",
                           notification.sender_id, notification.data);
                    break;
                    
                case NOTIFICATION_STATUS_UPDATE:
                    printf("[Receiver] Status update from Sender %lu (data: %lu)\n",
                           notification.sender_id, notification.data);
                    break;
                    
                case NOTIFICATION_ERROR_OCCURRED:
                    printf("[Receiver] ERROR notification from Sender %lu (data: %lu)\n",
                           notification.sender_id, notification.data);
                    break;
                    
                default:
                    printf("[Receiver] Unknown notification type %lu from Sender %lu\n",
                           notification.notification_type, notification.sender_id);
                    break;
            }
            
            printf("[Receiver] Total notifications received: %lu\n", notifications_received);
            
        } else {
            printf("[Receiver] No notifications received within timeout period\n");
        }
    }
}

// =============================================================================
// SYSTEM MONITORING AND STATISTICS
// =============================================================================

/**
 * @brief Statistics reporting task - monitors system performance
 * 
 * This task demonstrates:
 * - System monitoring and statistics collection
 * - Performance metrics reporting
 * - Communication pattern analysis
 */
void statistics_task(void *param) {
    uint32_t report_counter = 0;
    
    printf("[Statistics] Starting statistics monitoring task\n");
    
    while (1) {
        report_counter++;
        
        printf("\n=== COMMUNICATION STATISTICS REPORT #%lu ===\n", report_counter);
        printf("System uptime: %lu ms\n", pico_rtos_get_uptime_ms());
        
        // Queue-based communication stats
        printf("\nQueue-based Producer-Consumer:\n");
        printf("  Packets produced: %lu\n", packets_produced);
        printf("  Packets consumed: %lu\n", packets_consumed);
        printf("  Queue utilization: %s\n", 
               pico_rtos_queue_is_empty(&data_queue) ? "Empty" :
               pico_rtos_queue_is_full(&data_queue) ? "Full" : "Partial");
        
        // Semaphore-based resource sharing stats
        printf("\nSemaphore-based Resource Sharing:\n");
        printf("  Total resource accesses: %lu\n", resource_accesses);
        printf("  Current shared resource value: %lu\n", shared_resource_value);
        printf("  Semaphore available: %s\n", 
               pico_rtos_semaphore_is_available(&resource_semaphore) ? "Yes" : "No");
        
        // Task notification stats
        printf("\nTask Notification System:\n");
        printf("  Notifications sent: %lu\n", notifications_sent);
        printf("  Notifications received: %lu\n", notifications_received);
        printf("  Notification queue status: %s\n",
               pico_rtos_queue_is_empty(&notification_queue) ? "Empty" :
               pico_rtos_queue_is_full(&notification_queue) ? "Full" : "Partial");
        
        printf("==========================================\n\n");
        
        // Report every 10 seconds
        pico_rtos_task_delay(10000);
    }
}

/**
 * @brief LED indicator task - provides visual system status
 */
void led_indicator_task(void *param) {
    bool led_state = false;
    
    while (1) {
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        
        // Blink rate indicates system activity
        pico_rtos_task_delay(led_state ? 100 : 900);
    }
}

// =============================================================================
// MAIN FUNCTION AND INITIALIZATION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize GPIO for LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    printf("\n=== Pico-RTOS Multi-Task Communication Patterns Example ===\n");
    printf("This example demonstrates:\n");
    printf("1. Queue-based producer-consumer pattern\n");
    printf("2. Semaphore-based resource sharing\n");
    printf("3. Task notification lightweight messaging\n\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize communication objects
    printf("Initializing communication objects...\n");
    
    // Initialize queue for producer-consumer pattern
    if (!pico_rtos_queue_init(&data_queue, queue_buffer, sizeof(data_packet_t), QUEUE_SIZE)) {
        printf("ERROR: Failed to initialize data queue\n");
        return -1;
    }
    
    // Initialize semaphore for resource sharing (3 concurrent resource slots)
    if (!pico_rtos_semaphore_init(&resource_semaphore, SHARED_RESOURCE_COUNT, SHARED_RESOURCE_COUNT)) {
        printf("ERROR: Failed to initialize resource semaphore\n");
        return -1;
    }
    
    // Initialize mutex for shared resource protection
    if (!pico_rtos_mutex_init(&shared_resource_mutex)) {
        printf("ERROR: Failed to initialize shared resource mutex\n");
        return -1;
    }
    
    // Initialize notification queue
    if (!pico_rtos_queue_init(&notification_queue, notification_buffer, sizeof(notification_t), NOTIFICATION_QUEUE_SIZE)) {
        printf("ERROR: Failed to initialize notification queue\n");
        return -1;
    }
    
    printf("Communication objects initialized successfully\n\n");
    
    // Create tasks for different communication patterns
    printf("Creating tasks...\n");
    
    // Producer-Consumer tasks
    static pico_rtos_task_t producer1_task, producer2_task;
    static pico_rtos_task_t consumer1_task, consumer2_task;
    
    if (!pico_rtos_task_create(&producer1_task, "Producer1", producer_task, (void*)1, 1024, 3) ||
        !pico_rtos_task_create(&producer2_task, "Producer2", producer_task, (void*)2, 1024, 3)) {
        printf("ERROR: Failed to create producer tasks\n");
        return -1;
    }
    
    if (!pico_rtos_task_create(&consumer1_task, "Consumer1", consumer_task, (void*)1, 1024, 2) ||
        !pico_rtos_task_create(&consumer2_task, "Consumer2", consumer_task, (void*)2, 1024, 2)) {
        printf("ERROR: Failed to create consumer tasks\n");
        return -1;
    }
    
    // Resource sharing tasks
    static pico_rtos_task_t worker1_task, worker2_task, worker3_task, worker4_task;
    
    if (!pico_rtos_task_create(&worker1_task, "Worker1", resource_worker_task, (void*)1, 1024, 2) ||
        !pico_rtos_task_create(&worker2_task, "Worker2", resource_worker_task, (void*)2, 1024, 2) ||
        !pico_rtos_task_create(&worker3_task, "Worker3", resource_worker_task, (void*)3, 1024, 2) ||
        !pico_rtos_task_create(&worker4_task, "Worker4", resource_worker_task, (void*)4, 1024, 2)) {
        printf("ERROR: Failed to create resource worker tasks\n");
        return -1;
    }
    
    // Notification tasks
    static pico_rtos_task_t sender1_task, sender2_task, receiver_task;
    
    if (!pico_rtos_task_create(&sender1_task, "Sender1", notification_sender_task, (void*)1, 1024, 2) ||
        !pico_rtos_task_create(&sender2_task, "Sender2", notification_sender_task, (void*)2, 1024, 2)) {
        printf("ERROR: Failed to create notification sender tasks\n");
        return -1;
    }
    
    if (!pico_rtos_task_create(&receiver_task, "Receiver", notification_receiver_task, NULL, 1024, 3)) {
        printf("ERROR: Failed to create notification receiver task\n");
        return -1;
    }
    
    // System monitoring tasks
    static pico_rtos_task_t stats_task, led_task;
    
    if (!pico_rtos_task_create(&stats_task, "Statistics", statistics_task, NULL, 1024, 1)) {
        printf("ERROR: Failed to create statistics task\n");
        return -1;
    }
    
    if (!pico_rtos_task_create(&led_task, "LED", led_indicator_task, NULL, 256, 1)) {
        printf("ERROR: Failed to create LED indicator task\n");
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