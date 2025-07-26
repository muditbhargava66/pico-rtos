# Multi-Core Programming Guide for Pico-RTOS v0.3.0

This guide provides comprehensive information on developing multi-core applications using Pico-RTOS v0.3.0's dual-core support for the Raspberry Pi Pico (RP2040).

## Overview

Pico-RTOS v0.3.0 introduces symmetric multi-processing (SMP) support that allows tasks to run on both cores of the RP2040 simultaneously. This enables better CPU utilization and improved performance for multi-threaded applications.

### Key Features

- **Symmetric Multi-Processing**: Both cores can run tasks with full RTOS functionality
- **Task Affinity**: Bind tasks to specific cores or allow flexible scheduling
- **Inter-Core Communication**: Efficient message passing between cores
- **Load Balancing**: Automatic task distribution for optimal performance
- **Thread-Safe Synchronization**: All RTOS primitives work across cores

## Getting Started with Multi-Core

### Enabling Multi-Core Support

Multi-core support is enabled through the configuration system:

```c
// In your configuration or main.c
#define PICO_RTOS_ENABLE_MULTICORE  1
#define PICO_RTOS_ENABLE_LOAD_BALANCING  1

// Initialize multi-core support
bool pico_rtos_multicore_init(void) {
    // Initialize IPC system
    if (!pico_rtos_ipc_init()) {
        return false;
    }
    
    // Enable load balancing
    pico_rtos_smp_enable_load_balancing(true);
    pico_rtos_smp_set_load_balance_threshold(20);  // 20% imbalance threshold
    
    return true;
}
```

### Basic Multi-Core Application Structure

```c
#include "pico_rtos.h"

// Task functions
void core0_task(void *param);
void core1_task(void *param);
void shared_task(void *param);

int main() {
    // Initialize RTOS
    pico_rtos_init();
    
    // Initialize multi-core support
    pico_rtos_multicore_init();
    
    // Create tasks with different affinities
    pico_rtos_task_t core0_task_handle, core1_task_handle, shared_task_handle;
    
    // Task bound to core 0
    pico_rtos_task_create(&core0_task_handle, "core0_task", core0_task, NULL, 1024, 5);
    pico_rtos_task_set_core_affinity(&core0_task_handle, PICO_RTOS_CORE_AFFINITY_CORE0);
    
    // Task bound to core 1
    pico_rtos_task_create(&core1_task_handle, "core1_task", core1_task, NULL, 1024, 5);
    pico_rtos_task_set_core_affinity(&core1_task_handle, PICO_RTOS_CORE_AFFINITY_CORE1);
    
    // Task that can run on any core
    pico_rtos_task_create(&shared_task_handle, "shared_task", shared_task, NULL, 1024, 3);
    pico_rtos_task_set_core_affinity(&shared_task_handle, PICO_RTOS_CORE_AFFINITY_ANY);
    
    // Start the scheduler
    pico_rtos_start();
    
    return 0;
}
```

## Task Affinity Strategies

### Core Affinity Types

```c
typedef enum {
    PICO_RTOS_CORE_AFFINITY_NONE,    // No preference (legacy behavior)
    PICO_RTOS_CORE_AFFINITY_CORE0,   // Must run on core 0
    PICO_RTOS_CORE_AFFINITY_CORE1,   // Must run on core 1
    PICO_RTOS_CORE_AFFINITY_ANY      // Can run on any available core
} pico_rtos_core_affinity_t;
```

### When to Use Each Affinity Type

#### Core-Specific Tasks (CORE0/CORE1)
Use when tasks need:
- Dedicated CPU resources
- Predictable timing
- Hardware resource exclusivity
- Minimal inter-core synchronization overhead

```c
// Example: Time-critical sensor processing on core 1
void sensor_processing_task(void *param) {
    while (1) {
        // High-frequency sensor data processing
        // Benefits from dedicated core resources
        process_sensor_data();
        pico_rtos_task_delay(1);  // 1ms processing cycle
    }
}

// Bind to core 1 for consistent timing
pico_rtos_task_set_core_affinity(&sensor_task, PICO_RTOS_CORE_AFFINITY_CORE1);
```

#### Flexible Tasks (ANY)
Use for tasks that:
- Can benefit from load balancing
- Don't have strict timing requirements
- Perform general-purpose processing

```c
// Example: Background data logging
void logging_task(void *param) {
    while (1) {
        // Can run on either core as resources allow
        process_log_queue();
        pico_rtos_task_delay(100);  // 100ms processing cycle
    }
}

// Allow load balancer to optimize placement
pico_rtos_task_set_core_affinity(&logging_task, PICO_RTOS_CORE_AFFINITY_ANY);
```

## Inter-Core Communication

### Message-Based Communication

The recommended approach for inter-core communication is message passing:

```c
// Define message types
#define MSG_TYPE_SENSOR_DATA    1
#define MSG_TYPE_CONTROL_CMD    2
#define MSG_TYPE_STATUS_UPDATE  3

// Message structures
typedef struct {
    uint32_t sensor_id;
    float temperature;
    float humidity;
    uint32_t timestamp;
} sensor_data_msg_t;

typedef struct {
    uint32_t device_id;
    uint32_t command;
    uint32_t parameter;
} control_cmd_msg_t;

// Sending messages
void core0_producer_task(void *param) {
    sensor_data_msg_t sensor_data;
    
    while (1) {
        // Collect sensor data
        sensor_data.sensor_id = 1;
        sensor_data.temperature = read_temperature();
        sensor_data.humidity = read_humidity();
        sensor_data.timestamp = pico_rtos_get_tick_count();
        
        // Send to core 1
        bool sent = pico_rtos_ipc_send_message(1,                    // Target core 1
                                               MSG_TYPE_SENSOR_DATA, // Message type
                                               &sensor_data,         // Data
                                               sizeof(sensor_data),  // Size
                                               1000);                // 1s timeout
        
        if (!sent) {
            // Handle send failure
            printf("Failed to send sensor data to core 1\n");
        }
        
        pico_rtos_task_delay(100);  // 100ms sampling rate
    }
}

// Receiving messages
void core1_consumer_task(void *param) {
    uint32_t source_core, message_type, actual_length;
    sensor_data_msg_t received_data;
    
    while (1) {
        bool received = pico_rtos_ipc_receive_message(&source_core,
                                                      &message_type,
                                                      &received_data,
                                                      sizeof(received_data),
                                                      &actual_length,
                                                      5000);  // 5s timeout
        
        if (received) {
            switch (message_type) {
                case MSG_TYPE_SENSOR_DATA:
                    printf("Received sensor data from core %u: temp=%.1f°C, humidity=%.1f%%\n",
                           source_core, received_data.temperature, received_data.humidity);
                    process_sensor_data(&received_data);
                    break;
                    
                default:
                    printf("Unknown message type: %u\n", message_type);
                    break;
            }
        } else {
            // Handle timeout or error
            printf("No message received within timeout\n");
        }
    }
}
```

### Shared Memory Communication

For high-performance scenarios, you can use shared memory with proper synchronization:

```c
// Shared data structure
typedef struct {
    volatile uint32_t write_index;
    volatile uint32_t read_index;
    volatile bool data_ready;
    sensor_data_t buffer[BUFFER_SIZE];
    pico_rtos_mutex_t access_mutex;
} shared_sensor_buffer_t;

// Global shared buffer (accessible by both cores)
static shared_sensor_buffer_t g_shared_buffer;

// Producer (Core 0)
void core0_producer_task(void *param) {
    while (1) {
        sensor_data_t data;
        collect_sensor_data(&data);
        
        // Acquire exclusive access
        if (pico_rtos_mutex_lock(&g_shared_buffer.access_mutex, 100)) {
            // Write data to shared buffer
            uint32_t next_write = (g_shared_buffer.write_index + 1) % BUFFER_SIZE;
            if (next_write != g_shared_buffer.read_index) {
                g_shared_buffer.buffer[g_shared_buffer.write_index] = data;
                g_shared_buffer.write_index = next_write;
                g_shared_buffer.data_ready = true;
            }
            
            pico_rtos_mutex_unlock(&g_shared_buffer.access_mutex);
        }
        
        pico_rtos_task_delay(10);
    }
}

// Consumer (Core 1)
void core1_consumer_task(void *param) {
    while (1) {
        if (g_shared_buffer.data_ready) {
            if (pico_rtos_mutex_lock(&g_shared_buffer.access_mutex, 100)) {
                // Read data from shared buffer
                if (g_shared_buffer.read_index != g_shared_buffer.write_index) {
                    sensor_data_t data = g_shared_buffer.buffer[g_shared_buffer.read_index];
                    g_shared_buffer.read_index = (g_shared_buffer.read_index + 1) % BUFFER_SIZE;
                    
                    if (g_shared_buffer.read_index == g_shared_buffer.write_index) {
                        g_shared_buffer.data_ready = false;
                    }
                    
                    pico_rtos_mutex_unlock(&g_shared_buffer.access_mutex);
                    
                    // Process the data
                    process_sensor_data(&data);
                } else {
                    pico_rtos_mutex_unlock(&g_shared_buffer.access_mutex);
                }
            }
        }
        
        pico_rtos_task_delay(5);
    }
}
```

## Load Balancing

### Automatic Load Balancing

Enable automatic load balancing to optimize CPU utilization:

```c
// Enable load balancing with 15% threshold
pico_rtos_smp_enable_load_balancing(true);
pico_rtos_smp_set_load_balance_threshold(15);

// Tasks with PICO_RTOS_CORE_AFFINITY_ANY will be automatically balanced
```

### Manual Load Monitoring

Monitor core utilization and manually adjust task placement:

```c
void system_monitor_task(void *param) {
    pico_rtos_core_stats_t core0_stats, core1_stats;
    
    while (1) {
        // Get statistics for both cores
        pico_rtos_smp_get_core_stats(0, &core0_stats);
        pico_rtos_smp_get_core_stats(1, &core1_stats);
        
        printf("Core 0: %u%% CPU, %u tasks, %u context switches\n",
               core0_stats.cpu_utilization,
               core0_stats.active_tasks,
               core0_stats.context_switches);
               
        printf("Core 1: %u%% CPU, %u tasks, %u context switches\n",
               core1_stats.cpu_utilization,
               core1_stats.active_tasks,
               core1_stats.context_switches);
        
        // Check for significant imbalance
        int32_t imbalance = (int32_t)core0_stats.cpu_utilization - 
                           (int32_t)core1_stats.cpu_utilization;
        
        if (abs(imbalance) > 30) {
            printf("Warning: Core imbalance detected (%d%%)\n", imbalance);
            // Consider manual task migration or affinity adjustment
        }
        
        pico_rtos_task_delay(5000);  // Monitor every 5 seconds
    }
}
```

## Synchronization Across Cores

### Thread-Safe Primitives

All RTOS synchronization primitives are thread-safe across cores:

```c
// Shared resources
pico_rtos_mutex_t shared_resource_mutex;
pico_rtos_semaphore_t data_ready_semaphore;
pico_rtos_queue_t inter_core_queue;

// Core 0 task
void core0_task(void *param) {
    while (1) {
        // Acquire shared resource (works across cores)
        if (pico_rtos_mutex_lock(&shared_resource_mutex, 1000)) {
            // Access shared resource safely
            access_shared_resource();
            pico_rtos_mutex_unlock(&shared_resource_mutex);
            
            // Signal data ready to other core
            pico_rtos_semaphore_give(&data_ready_semaphore);
        }
        
        pico_rtos_task_delay(100);
    }
}

// Core 1 task
void core1_task(void *param) {
    while (1) {
        // Wait for data ready signal from other core
        if (pico_rtos_semaphore_take(&data_ready_semaphore, 2000)) {
            // Process the data
            process_shared_data();
        }
        
        pico_rtos_task_delay(50);
    }
}
```

### Event Groups for Multi-Core Coordination

Use event groups for complex multi-core synchronization:

```c
// Multi-core event coordination
pico_rtos_event_group_t system_events;

#define EVENT_CORE0_READY    (1 << 0)
#define EVENT_CORE1_READY    (1 << 1)
#define EVENT_DATA_PROCESSED (1 << 2)
#define EVENT_SHUTDOWN       (1 << 3)

// Core 0 initialization task
void core0_init_task(void *param) {
    // Perform core 0 specific initialization
    initialize_core0_resources();
    
    // Signal that core 0 is ready
    pico_rtos_event_group_set_bits(&system_events, EVENT_CORE0_READY);
    
    // Wait for both cores to be ready
    uint32_t events = pico_rtos_event_group_wait_bits(&system_events,
                                                      EVENT_CORE0_READY | EVENT_CORE1_READY,
                                                      true,   // Wait for ALL
                                                      false,  // Don't clear
                                                      10000); // 10s timeout
    
    if (events == (EVENT_CORE0_READY | EVENT_CORE1_READY)) {
        printf("Both cores initialized, starting main processing\n");
        start_main_processing();
    }
    
    // Task continues with main processing...
}

// Core 1 initialization task
void core1_init_task(void *param) {
    // Perform core 1 specific initialization
    initialize_core1_resources();
    
    // Signal that core 1 is ready
    pico_rtos_event_group_set_bits(&system_events, EVENT_CORE1_READY);
    
    // Wait for both cores to be ready
    uint32_t events = pico_rtos_event_group_wait_bits(&system_events,
                                                      EVENT_CORE0_READY | EVENT_CORE1_READY,
                                                      true,   // Wait for ALL
                                                      false,  // Don't clear
                                                      10000); // 10s timeout
    
    if (events == (EVENT_CORE0_READY | EVENT_CORE1_READY)) {
        printf("Both cores initialized, starting auxiliary processing\n");
        start_auxiliary_processing();
    }
    
    // Task continues with auxiliary processing...
}
```

## Performance Optimization

### Core Assignment Strategies

#### Strategy 1: Functional Separation
Assign different functional areas to different cores:

```c
// Core 0: Real-time control and sensor processing
void core0_control_task(void *param) {
    pico_rtos_task_set_core_affinity(NULL, PICO_RTOS_CORE_AFFINITY_CORE0);
    
    while (1) {
        read_sensors();
        update_control_outputs();
        pico_rtos_task_delay(1);  // 1ms control loop
    }
}

// Core 1: Communication and user interface
void core1_comm_task(void *param) {
    pico_rtos_task_set_core_affinity(NULL, PICO_RTOS_CORE_AFFINITY_CORE1);
    
    while (1) {
        handle_uart_communication();
        update_display();
        process_user_input();
        pico_rtos_task_delay(10);  // 10ms UI update
    }
}
```

#### Strategy 2: Pipeline Processing
Use cores as stages in a processing pipeline:

```c
// Core 0: Data acquisition stage
void data_acquisition_task(void *param) {
    pico_rtos_task_set_core_affinity(NULL, PICO_RTOS_CORE_AFFINITY_CORE0);
    
    while (1) {
        raw_data_t raw_data;
        acquire_raw_data(&raw_data);
        
        // Send to processing stage on core 1
        pico_rtos_ipc_send_message(1, MSG_TYPE_RAW_DATA, &raw_data, 
                                   sizeof(raw_data), 100);
        
        pico_rtos_task_delay(5);
    }
}

// Core 1: Data processing stage
void data_processing_task(void *param) {
    pico_rtos_task_set_core_affinity(NULL, PICO_RTOS_CORE_AFFINITY_CORE1);
    
    while (1) {
        uint32_t source_core, message_type, actual_length;
        raw_data_t raw_data;
        processed_data_t processed_data;
        
        if (pico_rtos_ipc_receive_message(&source_core, &message_type,
                                          &raw_data, sizeof(raw_data),
                                          &actual_length, 1000)) {
            
            // Process the data
            process_data(&raw_data, &processed_data);
            
            // Send to output stage or store results
            store_processed_data(&processed_data);
        }
    }
}
```

### Memory Considerations

#### Cache Coherency
The RP2040 doesn't have hardware cache coherency, so be careful with shared memory:

```c
// Use volatile for shared variables
volatile uint32_t shared_counter = 0;

// Use memory barriers when necessary
__dmb();  // Data memory barrier
__dsb();  // Data synchronization barrier
```

#### Memory Pool Optimization
Use separate memory pools for each core to reduce contention:

```c
// Core-specific memory pools
static uint8_t core0_pool_memory[POOL_SIZE];
static uint8_t core1_pool_memory[POOL_SIZE];
static pico_rtos_memory_pool_t core0_pool, core1_pool;

void initialize_core_pools(void) {
    pico_rtos_memory_pool_init(&core0_pool, core0_pool_memory, BLOCK_SIZE, BLOCK_COUNT);
    pico_rtos_memory_pool_init(&core1_pool, core1_pool_memory, BLOCK_SIZE, BLOCK_COUNT);
}

// Tasks use their core-specific pool
void core0_task(void *param) {
    void *block = pico_rtos_memory_pool_alloc(&core0_pool, 100);
    // Use block...
    pico_rtos_memory_pool_free(&core0_pool, block);
}
```

## Debugging Multi-Core Applications

### Core-Specific Debugging

Use core-aware debugging techniques:

```c
void debug_task_info(void) {
    pico_rtos_task_info_t task_info;
    uint32_t current_core = pico_rtos_get_current_core();
    
    if (pico_rtos_get_task_info(NULL, &task_info)) {
        printf("Task '%s' on core %u:\n", task_info.name, current_core);
        printf("  Assigned core: %u\n", task_info.assigned_core);
        printf("  CPU time: %llu μs\n", task_info.cpu_time_us);
        printf("  Context switches: %u\n", task_info.context_switches);
    }
}
```

### Inter-Core Communication Debugging

Monitor IPC performance and reliability:

```c
typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t send_failures;
    uint32_t receive_timeouts;
    uint32_t total_latency_us;
} ipc_stats_t;

static ipc_stats_t ipc_stats = {0};

// Enhanced IPC send with statistics
bool debug_ipc_send_message(uint32_t target_core, uint32_t message_type,
                           const void *data, uint32_t data_length, uint32_t timeout) {
    uint32_t start_time = pico_rtos_get_tick_count();
    
    bool result = pico_rtos_ipc_send_message(target_core, message_type, 
                                            data, data_length, timeout);
    
    uint32_t end_time = pico_rtos_get_tick_count();
    
    if (result) {
        ipc_stats.messages_sent++;
        ipc_stats.total_latency_us += (end_time - start_time) * 1000;
    } else {
        ipc_stats.send_failures++;
    }
    
    return result;
}

// Print IPC statistics
void print_ipc_stats(void) {
    printf("IPC Statistics:\n");
    printf("  Messages sent: %u\n", ipc_stats.messages_sent);
    printf("  Messages received: %u\n", ipc_stats.messages_received);
    printf("  Send failures: %u\n", ipc_stats.send_failures);
    printf("  Receive timeouts: %u\n", ipc_stats.receive_timeouts);
    
    if (ipc_stats.messages_sent > 0) {
        uint32_t avg_latency = ipc_stats.total_latency_us / ipc_stats.messages_sent;
        printf("  Average send latency: %u μs\n", avg_latency);
    }
}
```

## Best Practices

### 1. Task Design
- Keep tasks loosely coupled between cores
- Use message passing instead of shared memory when possible
- Design for graceful degradation if one core fails

### 2. Resource Management
- Use core-specific resources when possible
- Minimize cross-core synchronization overhead
- Consider NUMA-like memory access patterns

### 3. Performance Monitoring
- Regularly monitor core utilization balance
- Track inter-core communication latency
- Use profiling tools to identify bottlenecks

### 4. Error Handling
- Implement core failure detection
- Design recovery mechanisms for core failures
- Use timeouts for all inter-core operations

### 5. Testing
- Test with various load patterns
- Verify behavior under high inter-core communication load
- Test core failure scenarios

## Common Pitfalls and Solutions

### Pitfall 1: Excessive Inter-Core Communication
**Problem**: Too much communication between cores causes performance degradation.

**Solution**: Batch messages and use appropriate buffer sizes:
```c
// Instead of sending individual sensor readings
// Batch multiple readings together
typedef struct {
    sensor_reading_t readings[BATCH_SIZE];
    uint32_t count;
} sensor_batch_t;
```

### Pitfall 2: Unbalanced Core Utilization
**Problem**: One core is overloaded while the other is idle.

**Solution**: Use load balancing and monitor core statistics:
```c
// Enable automatic load balancing
pico_rtos_smp_enable_load_balancing(true);

// Monitor and adjust manually if needed
void monitor_and_rebalance(void) {
    pico_rtos_core_stats_t stats[2];
    pico_rtos_smp_get_core_stats(0, &stats[0]);
    pico_rtos_smp_get_core_stats(1, &stats[1]);
    
    if (abs((int)stats[0].cpu_utilization - (int)stats[1].cpu_utilization) > 25) {
        // Consider task migration or affinity changes
        rebalance_tasks();
    }
}
```

### Pitfall 3: Deadlocks in Cross-Core Synchronization
**Problem**: Tasks on different cores waiting for each other's resources.

**Solution**: Use consistent lock ordering and timeouts:
```c
// Always acquire locks in the same order
bool acquire_dual_locks(pico_rtos_mutex_t *lock1, pico_rtos_mutex_t *lock2) {
    // Order locks by address to prevent deadlock
    pico_rtos_mutex_t *first = (lock1 < lock2) ? lock1 : lock2;
    pico_rtos_mutex_t *second = (lock1 < lock2) ? lock2 : lock1;
    
    if (pico_rtos_mutex_lock(first, 1000)) {
        if (pico_rtos_mutex_lock(second, 1000)) {
            return true;
        }
        pico_rtos_mutex_unlock(first);
    }
    return false;
}
```

This guide provides a comprehensive foundation for developing efficient multi-core applications with Pico-RTOS v0.3.0. Remember to always test thoroughly and monitor performance in your specific use case.