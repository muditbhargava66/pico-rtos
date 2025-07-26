# Performance Tuning Guide for Pico-RTOS v0.3.0

This guide provides comprehensive performance optimization strategies for Pico-RTOS v0.3.0, covering both single-core and multi-core configurations.

## Performance Overview

Pico-RTOS v0.3.0 introduces several performance enhancements while maintaining the real-time characteristics of previous versions:

- **Multi-core support** for improved throughput
- **Advanced synchronization primitives** with optimized implementations
- **Memory pools** for deterministic allocation
- **Stream buffers** for efficient data streaming
- **Comprehensive profiling tools** for performance analysis

## Performance Characteristics

### Core RTOS Performance

| Operation | Typical Latency | Notes |
|-----------|----------------|-------|
| Task switch | 2-5 μs | Context-dependent |
| Mutex lock/unlock | 1-3 μs | Uncontended |
| Queue send/receive | 2-4 μs | Small messages |
| Semaphore give/take | 1-2 μs | Uncontended |
| Timer operations | 1-3 μs | Software timers |

### New v0.3.0 Features Performance

| Feature | Typical Latency | Memory Overhead | Notes |
|---------|----------------|-----------------|-------|
| Event Groups | 2-5 μs | 48 bytes + blocking | O(1) operations |
| Stream Buffers | 5-15 μs | Buffer size + 64 bytes | Variable message size |
| Memory Pools | 1-3 μs | 32 bytes + pool size | O(1) allocation |
| Multi-Core IPC | 5-15 μs | 256 bytes per channel | Hardware FIFO based |
| High-Res Timers | 1-2 μs | 48 bytes per timer | Microsecond resolution |
| Task Profiling | 1-2 μs | 32 bytes per function | Per entry/exit |
| Event Tracing | 0.5-1 μs | 16 bytes per event | Minimal overhead |

## Configuration Optimization

### Memory-Constrained Systems

For systems with limited RAM, optimize configuration:

```cmake
# Minimal memory configuration
set(PICO_RTOS_MAX_TASKS 16)                    # Reduce from default 32
set(PICO_RTOS_MAX_MUTEXES 8)                   # Reduce from default 16
set(PICO_RTOS_MAX_QUEUES 8)                    # Reduce from default 16
set(PICO_RTOS_MAX_SEMAPHORES 8)                # Reduce from default 16
set(PICO_RTOS_MAX_TIMERS 4)                    # Reduce from default 8

# Disable unused features
set(PICO_RTOS_ENABLE_EVENT_GROUPS OFF)
set(PICO_RTOS_ENABLE_STREAM_BUFFERS OFF)
set(PICO_RTOS_ENABLE_MEMORY_POOLS OFF)
set(PICO_RTOS_ENABLE_MULTICORE OFF)
set(PICO_RTOS_ENABLE_PROFILING OFF)
set(PICO_RTOS_ENABLE_TRACING OFF)

# Optimize stack sizes
set(PICO_RTOS_IDLE_TASK_STACK_SIZE 128)        # Reduce from default 256
set(PICO_RTOS_TIMER_TASK_STACK_SIZE 256)       # Reduce from default 512
```

### Performance-Optimized Systems

For maximum performance with adequate memory:

```cmake
# Performance-optimized configuration
set(PICO_RTOS_MAX_TASKS 64)                    # Increase for more parallelism
set(PICO_RTOS_TICK_RATE_HZ 2000)               # Higher resolution timing
set(PICO_RTOS_ENABLE_FAST_CONTEXT_SWITCH ON)   # Optimize context switching

# Enable all performance features
set(PICO_RTOS_ENABLE_EVENT_GROUPS ON)
set(PICO_RTOS_ENABLE_STREAM_BUFFERS ON)
set(PICO_RTOS_ENABLE_MEMORY_POOLS ON)
set(PICO_RTOS_ENABLE_MULTICORE ON)
set(PICO_RTOS_ENABLE_LOAD_BALANCING ON)
set(PICO_RTOS_ENABLE_HIRES_TIMERS ON)

# Optimize buffer sizes
set(PICO_RTOS_MAX_EVENT_GROUPS 32)
set(PICO_RTOS_MAX_STREAM_BUFFERS 16)
set(PICO_RTOS_MAX_MEMORY_POOLS 8)
set(PICO_RTOS_MAX_HIRES_TIMERS 32)
```

## Task Design Optimization

### Task Priority Assignment

Optimal priority assignment is crucial for real-time performance:

```c
// Priority levels (higher number = higher priority)
#define PRIORITY_CRITICAL    10    // Interrupt-like tasks
#define PRIORITY_HIGH        7     // Real-time control
#define PRIORITY_NORMAL      5     // Regular processing
#define PRIORITY_LOW         3     // Background tasks
#define PRIORITY_IDLE        1     // Cleanup tasks

// Example priority assignment
pico_rtos_task_create(&interrupt_task, "IntTask", interrupt_handler, NULL, 512, PRIORITY_CRITICAL);
pico_rtos_task_create(&control_task, "Control", control_loop, NULL, 1024, PRIORITY_HIGH);
pico_rtos_task_create(&data_task, "DataProc", data_processing, NULL, 1024, PRIORITY_NORMAL);
pico_rtos_task_create(&comm_task, "Comm", communication, NULL, 1024, PRIORITY_LOW);
```

### Stack Size Optimization

Right-size stack allocations to avoid waste and overflow:

```c
// Stack size guidelines
#define STACK_SIZE_MINIMAL   256    // Simple tasks, minimal local variables
#define STACK_SIZE_SMALL     512    // Basic processing, small buffers
#define STACK_SIZE_MEDIUM    1024   // Standard processing, moderate buffers
#define STACK_SIZE_LARGE     2048   // Complex processing, large buffers
#define STACK_SIZE_XLARGE    4096   // Very complex processing, deep call stacks

// Monitor stack usage
void monitor_stack_usage(void) {
    pico_rtos_task_info_t task_info;
    if (pico_rtos_get_task_info(NULL, &task_info)) {
        float usage_percent = (float)task_info.stack_usage / task_info.stack_high_water * 100.0f;
        if (usage_percent > 80.0f) {
            printf("WARNING: Task %s stack usage high: %.1f%%\n", 
                   task_info.name, usage_percent);
        }
    }
}
```

### Task Timing Optimization

Design tasks for optimal timing characteristics:

```c
// Good: Predictable timing
void well_designed_task(void *param) {
    while (1) {
        // Fixed-time operations
        read_sensors();           // ~100 μs
        process_data();          // ~200 μs
        update_outputs();        // ~50 μs
        
        // Predictable delay
        pico_rtos_task_delay(10); // 10ms period
    }
}

// Avoid: Variable timing
void poorly_designed_task(void *param) {
    while (1) {
        // Variable-time operations
        while (data_available()) {    // Unpredictable loop
            process_variable_data();  // Variable processing time
        }
        
        // Variable delay based on processing
        pico_rtos_task_delay(processing_time_dependent_delay);
    }
}
```

## Synchronization Optimization

### Choosing the Right Primitive

Select synchronization primitives based on use case:

```c
// Use mutexes for resource protection
pico_rtos_mutex_t resource_mutex;
void access_shared_resource(void) {
    if (pico_rtos_mutex_lock(&resource_mutex, 1000)) {
        // Access shared resource
        pico_rtos_mutex_unlock(&resource_mutex);
    }
}

// Use semaphores for resource counting
pico_rtos_semaphore_t buffer_semaphore;
pico_rtos_semaphore_init(&buffer_semaphore, 0, MAX_BUFFERS);

// Use event groups for complex coordination
pico_rtos_event_group_t system_events;
uint32_t events = pico_rtos_event_group_wait_bits(&system_events,
                                                  EVENT_A | EVENT_B,
                                                  true,    // wait for all
                                                  false,   // don't clear
                                                  5000);   // timeout

// Use queues for small, fixed-size messages
pico_rtos_queue_t command_queue;
command_t commands[QUEUE_SIZE];
pico_rtos_queue_init(&command_queue, commands, sizeof(command_t), QUEUE_SIZE);

// Use stream buffers for variable-size messages
pico_rtos_stream_buffer_t data_stream;
uint8_t stream_buffer[STREAM_SIZE];
pico_rtos_stream_buffer_init(&data_stream, stream_buffer, STREAM_SIZE);
```

### Lock Contention Minimization

Reduce lock contention for better performance:

```c
// Good: Minimal critical section
void optimized_critical_section(void) {
    // Prepare data outside critical section
    data_t local_data = prepare_data();
    
    // Minimal critical section
    pico_rtos_mutex_lock(&shared_mutex, 1000);
    shared_data = local_data;  // Quick copy
    pico_rtos_mutex_unlock(&shared_mutex);
    
    // Continue processing outside critical section
    post_process_data();
}

// Avoid: Long critical section
void unoptimized_critical_section(void) {
    pico_rtos_mutex_lock(&shared_mutex, 1000);
    
    // Long processing in critical section (BAD)
    data_t data = complex_data_preparation();
    shared_data = data;
    perform_complex_validation();
    
    pico_rtos_mutex_unlock(&shared_mutex);
}
```

## Memory Management Optimization

### Memory Pool Usage

Use memory pools for deterministic allocation:

```c
// Create memory pools for different allocation sizes
static uint8_t small_pool_memory[32 * 64];   // 64 blocks of 32 bytes
static uint8_t large_pool_memory[256 * 16];  // 16 blocks of 256 bytes

pico_rtos_memory_pool_t small_pool, large_pool;

void initialize_memory_pools(void) {
    pico_rtos_memory_pool_init(&small_pool, small_pool_memory, 32, 64);
    pico_rtos_memory_pool_init(&large_pool, large_pool_memory, 256, 16);
}

// Use appropriate pool based on size
void *allocate_buffer(size_t size) {
    if (size <= 32) {
        return pico_rtos_memory_pool_alloc(&small_pool, 1000);
    } else if (size <= 256) {
        return pico_rtos_memory_pool_alloc(&large_pool, 1000);
    }
    return NULL;  // Size too large
}
```

### Memory Usage Monitoring

Monitor memory usage to prevent leaks:

```c
void monitor_memory_usage(void) {
    uint32_t current, peak, allocations;
    pico_rtos_get_memory_stats(&current, &peak, &allocations);
    
    printf("Memory: %lu current, %lu peak, %lu allocations\n",
           current, peak, allocations);
    
    // Check memory pools
    pico_rtos_memory_pool_stats_t pool_stats;
    if (pico_rtos_memory_pool_get_stats(&small_pool, &pool_stats)) {
        printf("Small pool: %lu/%lu blocks used (%.1f%%)\n",
               pool_stats.blocks_allocated, pool_stats.total_blocks,
               (float)pool_stats.blocks_allocated / pool_stats.total_blocks * 100.0f);
    }
}
```

## Multi-Core Optimization

### Core Affinity Strategy

Optimize task distribution across cores:

```c
void optimize_core_assignment(void) {
    // Core 0: Time-critical tasks
    pico_rtos_task_set_core_affinity(&realtime_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    pico_rtos_task_set_core_affinity(&interrupt_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    
    // Core 1: Processing-intensive tasks
    pico_rtos_task_set_core_affinity(&data_processing_task, PICO_RTOS_CORE_AFFINITY_CORE1);
    pico_rtos_task_set_core_affinity(&communication_task, PICO_RTOS_CORE_AFFINITY_CORE1);
    
    // Flexible tasks: Allow load balancing
    pico_rtos_task_set_core_affinity(&background_task, PICO_RTOS_CORE_AFFINITY_ANY);
    pico_rtos_task_set_core_affinity(&logging_task, PICO_RTOS_CORE_AFFINITY_ANY);
    
    // Configure load balancing
    pico_rtos_smp_enable_load_balancing(true);
    pico_rtos_smp_set_load_balance_threshold(20);  // 20% imbalance threshold
}
```

### Inter-Core Communication Optimization

Optimize IPC for performance:

```c
// Batch messages to reduce IPC overhead
typedef struct {
    sensor_reading_t readings[BATCH_SIZE];
    uint32_t count;
} sensor_batch_t;

void send_sensor_batch(uint32_t target_core, const sensor_batch_t *batch) {
    // Single IPC call for multiple readings
    pico_rtos_ipc_send_message(target_core, MSG_TYPE_SENSOR_BATCH, 
                               batch, sizeof(sensor_batch_t), 1000);
}

// Use appropriate message sizes
void optimize_ipc_message_size(void) {
    // Good: Right-sized messages
    small_command_t cmd = {.id = 1, .param = 42};
    pico_rtos_ipc_send_message(1, MSG_TYPE_COMMAND, &cmd, sizeof(cmd), 1000);
    
    // Avoid: Oversized messages
    // Don't send large buffers via IPC - use shared memory instead
}
```

## I/O and Communication Optimization

### Stream Buffer Optimization

Optimize stream buffer usage for throughput:

```c
// Size stream buffers appropriately
#define STREAM_BUFFER_SIZE_SMALL    512     // For control messages
#define STREAM_BUFFER_SIZE_MEDIUM   2048    // For sensor data
#define STREAM_BUFFER_SIZE_LARGE    8192    // For bulk data transfer

// Use zero-copy operations for large data
void zero_copy_example(void) {
    uint32_t available_space;
    uint8_t *write_ptr = pico_rtos_stream_buffer_get_write_pointer(&data_stream, 
                                                                   &available_space);
    if (write_ptr && available_space >= data_size) {
        // Direct write to stream buffer (zero-copy)
        memcpy(write_ptr, large_data, data_size);
        pico_rtos_stream_buffer_commit_write(&data_stream, data_size);
    }
}
```

### High-Resolution Timer Optimization

Use high-resolution timers efficiently:

```c
// Group related timers to minimize overhead
void setup_timer_groups(void) {
    // Use one timer for multiple related timeouts
    pico_rtos_hires_timer_init(&sensor_timer, "SensorGroup", 
                               sensor_timer_callback, NULL, 1000, true);
    
    // Avoid creating many individual timers
    // Instead, use a single timer with a callback that handles multiple timeouts
}

void sensor_timer_callback(void *param) {
    static uint32_t counter = 0;
    counter++;
    
    // Handle different sensor rates with a single timer
    if (counter % 1 == 0) handle_fast_sensor();    // Every 1ms
    if (counter % 10 == 0) handle_medium_sensor();  // Every 10ms
    if (counter % 100 == 0) handle_slow_sensor();   // Every 100ms
}
```

## Debugging and Profiling Impact

### Production vs Development Builds

Configure debugging features based on build type:

```c
#ifdef DEBUG_BUILD
    // Development build - full debugging
    #define ENABLE_PROFILING        1
    #define ENABLE_TRACING          1
    #define ENABLE_TASK_INSPECTION  1
    #define PROFILER_MAX_ENTRIES    100
    #define TRACE_BUFFER_SIZE       1000
#else
    // Production build - minimal debugging
    #define ENABLE_PROFILING        0
    #define ENABLE_TRACING          0
    #define ENABLE_TASK_INSPECTION  0
    #define PROFILER_MAX_ENTRIES    0
    #define TRACE_BUFFER_SIZE       0
#endif

void configure_debugging(void) {
#if ENABLE_PROFILING
    pico_rtos_profiler_init(PROFILER_MAX_ENTRIES);
    pico_rtos_profiler_start();
#endif

#if ENABLE_TRACING
    pico_rtos_trace_init(TRACE_BUFFER_SIZE);
    pico_rtos_trace_start();
#endif
}
```

### Selective Profiling

Profile only critical functions to minimize overhead:

```c
// Profile only performance-critical functions
#define PROFILE_CRITICAL_FUNCTIONS 1

void critical_function(void) {
#if PROFILE_CRITICAL_FUNCTIONS
    PICO_RTOS_PROFILE_FUNCTION_START(FUNC_ID_CRITICAL);
#endif
    
    // Critical function implementation
    
#if PROFILE_CRITICAL_FUNCTIONS
    PICO_RTOS_PROFILE_FUNCTION_END(FUNC_ID_CRITICAL);
#endif
}

void non_critical_function(void) {
    // No profiling for non-critical functions
    // to minimize overhead
}
```

## Performance Monitoring

### Real-Time Performance Metrics

Monitor key performance indicators:

```c
typedef struct {
    uint32_t max_task_switch_time_us;
    uint32_t max_interrupt_latency_us;
    uint32_t cpu_utilization_percent;
    uint32_t memory_utilization_percent;
    uint32_t context_switches_per_second;
} performance_metrics_t;

void collect_performance_metrics(performance_metrics_t *metrics) {
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    
    metrics->cpu_utilization_percent = 
        100 - (stats.idle_counter * 100 / pico_rtos_get_uptime_ms());
    metrics->memory_utilization_percent = 
        stats.current_memory * 100 / stats.peak_memory;
    metrics->context_switches_per_second = 
        stats.context_switches * 1000 / pico_rtos_get_uptime_ms();
    
    // Set thresholds for performance warnings
    if (metrics->cpu_utilization_percent > 80) {
        printf("WARNING: High CPU utilization: %lu%%\n", 
               metrics->cpu_utilization_percent);
    }
    
    if (metrics->memory_utilization_percent > 90) {
        printf("WARNING: High memory utilization: %lu%%\n", 
               metrics->memory_utilization_percent);
    }
}
```

### Automated Performance Testing

Implement automated performance validation:

```c
void performance_validation_suite(void) {
    printf("Running performance validation suite...\n");
    
    // Test 1: Task switch latency
    uint32_t switch_latency = measure_task_switch_latency();
    assert(switch_latency < 10);  // Must be < 10 μs
    
    // Test 2: Mutex lock/unlock latency
    uint32_t mutex_latency = measure_mutex_latency();
    assert(mutex_latency < 5);    // Must be < 5 μs
    
    // Test 3: Queue throughput
    uint32_t queue_throughput = measure_queue_throughput();
    assert(queue_throughput > 10000);  // Must be > 10k msgs/sec
    
    // Test 4: Memory allocation latency
    uint32_t alloc_latency = measure_memory_pool_latency();
    assert(alloc_latency < 3);    // Must be < 3 μs
    
    printf("All performance tests passed\n");
}
```

## Platform-Specific Optimizations

### RP2040-Specific Optimizations

Leverage RP2040 hardware features:

```c
// Use hardware-specific optimizations
void rp2040_optimizations(void) {
    // Use both cores effectively
    pico_rtos_smp_enable_load_balancing(true);
    
    // Optimize for RP2040's dual-core architecture
    // Core 0: Real-time tasks
    // Core 1: Background processing
    
    // Use hardware timers for high-resolution timing
    pico_rtos_hires_timer_init(&precision_timer, "HWTimer", 
                               timer_callback, NULL, 100, true);  // 100 μs
    
    // Leverage hardware FIFOs for inter-core communication
    // (automatically used by IPC system)
}
```

## Performance Troubleshooting

### Common Performance Issues

#### Issue 1: High Task Switch Overhead

**Symptoms**: High CPU utilization, poor responsiveness
**Diagnosis**: 
```c
void diagnose_task_switching(void) {
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    
    uint32_t switches_per_ms = stats.context_switches / pico_rtos_get_uptime_ms();
    if (switches_per_ms > 100) {
        printf("WARNING: High task switch rate: %lu/ms\n", switches_per_ms);
    }
}
```

**Solutions**:
- Increase task delays to reduce switching frequency
- Combine related tasks
- Use event-driven design instead of polling

#### Issue 2: Memory Fragmentation

**Symptoms**: Allocation failures despite available memory
**Diagnosis**:
```c
void diagnose_memory_fragmentation(void) {
    uint32_t current, peak, allocations;
    pico_rtos_get_memory_stats(&current, &peak, &allocations);
    
    if (current < peak * 0.8f && allocations > peak * 10) {
        printf("WARNING: Possible memory fragmentation\n");
    }
}
```

**Solutions**:
- Use memory pools instead of malloc/free
- Allocate large buffers at startup
- Implement memory compaction if necessary

#### Issue 3: Lock Contention

**Symptoms**: Tasks blocked frequently, poor parallelism
**Diagnosis**:
```c
void diagnose_lock_contention(void) {
    pico_rtos_task_info_t tasks[32];
    uint32_t task_count = pico_rtos_get_all_task_info(tasks, 32);
    
    uint32_t blocked_tasks = 0;
    for (uint32_t i = 0; i < task_count; i++) {
        if (tasks[i].state == PICO_RTOS_TASK_STATE_BLOCKED) {
            blocked_tasks++;
        }
    }
    
    if (blocked_tasks > task_count / 2) {
        printf("WARNING: High lock contention - %lu/%lu tasks blocked\n",
               blocked_tasks, task_count);
    }
}
```

**Solutions**:
- Reduce critical section duration
- Use lock-free algorithms where possible
- Implement reader-writer locks for read-heavy scenarios

## Conclusion

Optimizing Pico-RTOS v0.3.0 performance requires a systematic approach considering configuration, task design, synchronization, memory management, and multi-core utilization. Use the profiling and monitoring tools to identify bottlenecks and validate optimizations. Remember that premature optimization can harm maintainability - profile first, then optimize based on actual measurements.