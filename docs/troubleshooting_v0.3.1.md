# Troubleshooting Guide for Pico-RTOS v0.3.1

This guide provides solutions to common issues encountered when using or migrating to Pico-RTOS v0.3.1.

## General Troubleshooting

### System Won't Start

**Symptoms**: System hangs during initialization, no output, or immediate crash

**Possible Causes & Solutions**:

1. **Insufficient Stack Size**
   ```c
   // Check if idle task stack is too small
   #define PICO_RTOS_IDLE_TASK_STACK_SIZE 512  // Increase from 256
   
   // Check individual task stack sizes
   pico_rtos_task_create(&task, "MyTask", task_func, NULL, 2048, 5);  // Increase stack
   ```

2. **Configuration Mismatch**
   ```cmake
   # Ensure all required features are enabled
   set(PICO_RTOS_ENABLE_MULTICORE ON)  # If using multi-core features
   set(PICO_RTOS_ENABLE_EVENT_GROUPS ON)  # If using event groups
   ```

3. **Hardware Timer Conflicts**
   ```c
   // Check for timer conflicts with other libraries
   // Ensure PICO_RTOS_TIMER_HARDWARE is set correctly
   ```

### Memory Issues

**Symptoms**: Allocation failures, system instability, crashes

**Diagnosis**:
```c
void diagnose_memory_issues(void) {
    uint32_t current, peak, allocations;
    pico_rtos_get_memory_stats(&current, &peak, &allocations);
    
    printf("Memory: %lu current, %lu peak, %lu total allocations\n",
           current, peak, allocations);
    
    if (current > 200 * 1024) {  // 200KB threshold for RP2040
        printf("WARNING: High memory usage\n");
    }
}
```

**Solutions**:
1. **Reduce Memory Usage**
   ```cmake
   # Reduce maximum objects
   set(PICO_RTOS_MAX_TASKS 16)        # Default: 32
   set(PICO_RTOS_MAX_MUTEXES 8)       # Default: 16
   set(PICO_RTOS_MAX_QUEUES 8)        # Default: 16
   ```

2. **Use Memory Pools**
   ```c
   // Replace malloc/free with memory pools
   static uint8_t pool_memory[1024 * 16];
   pico_rtos_memory_pool_t pool;
   pico_rtos_memory_pool_init(&pool, pool_memory, 64, 256);
   ```

3. **Optimize Stack Sizes**
   ```c
   // Monitor actual stack usage
   pico_rtos_task_info_t info;
   pico_rtos_get_task_info(&task, &info);
   printf("Stack usage: %lu/%lu (%.1f%%)\n", 
          info.stack_usage, info.stack_high_water,
          (float)info.stack_usage / info.stack_high_water * 100.0f);
   ```

## Multi-Core Issues

### IPC Initialization Failures

**Symptoms**: `pico_rtos_ipc_init()` returns false

**Solutions**:
```c
// Ensure multi-core is enabled in configuration
#if !defined(PICO_RTOS_ENABLE_MULTICORE) || !PICO_RTOS_ENABLE_MULTICORE
#error "Multi-core support not enabled"
#endif

// Initialize IPC before creating tasks
if (!pico_rtos_ipc_init()) {
    printf("ERROR: IPC initialization failed\n");
    // Check hardware FIFO availability
    // Ensure both cores are running
}
```

### Core Affinity Issues

**Symptoms**: Tasks not running on expected cores, load balancing not working

**Diagnosis**:
```c
void diagnose_core_affinity(void) {
    pico_rtos_task_info_t info;
    pico_rtos_get_task_info(&my_task, &info);
    
    printf("Task affinity: %lu, assigned core: %lu, current core: %lu\n",
           info.core_affinity, info.assigned_core, pico_rtos_get_current_core());
}
```

**Solutions**:
```c
// Ensure load balancing is properly configured
pico_rtos_smp_enable_load_balancing(true);
pico_rtos_smp_set_load_balance_threshold(20);

// Check core affinity settings
pico_rtos_core_affinity_t affinity = pico_rtos_task_get_core_affinity(&task);
if (affinity != PICO_RTOS_CORE_AFFINITY_ANY) {
    // Task is bound to specific core
    pico_rtos_task_set_core_affinity(&task, PICO_RTOS_CORE_AFFINITY_ANY);
}
```

### Inter-Core Communication Failures

**Symptoms**: IPC messages not received, timeouts, data corruption

**Diagnosis**:
```c
void diagnose_ipc_issues(void) {
    // Test basic IPC functionality
    uint32_t test_data = 0x12345678;
    bool sent = pico_rtos_ipc_send_message(1, 1, &test_data, sizeof(test_data), 1000);
    
    if (!sent) {
        printf("ERROR: IPC send failed\n");
        // Check if target core is running
        // Check message size limits
        // Check FIFO status
    }
}
```

**Solutions**:
1. **Check Message Size Limits**
   ```c
   // Ensure message size is within limits
   if (data_length > PICO_RTOS_IPC_MAX_MESSAGE_SIZE) {
       printf("ERROR: Message too large: %lu bytes\n", data_length);
       // Split message or use shared memory
   }
   ```

2. **Implement Proper Error Handling**
   ```c
   bool send_with_retry(uint32_t target_core, uint32_t type, 
                       const void *data, uint32_t length) {
       for (int retry = 0; retry < 3; retry++) {
           if (pico_rtos_ipc_send_message(target_core, type, data, length, 1000)) {
               return true;
           }
           pico_rtos_task_delay(10);  // Brief delay before retry
       }
       return false;
   }
   ```

## Synchronization Issues

### Deadlock Detection

**Symptoms**: System hangs, tasks stuck in blocked state

**Diagnosis**:
```c
void detect_deadlocks(void) {
    pico_rtos_task_info_t tasks[32];
    uint32_t task_count = pico_rtos_get_all_task_info(tasks, 32);
    
    uint32_t blocked_count = 0;
    for (uint32_t i = 0; i < task_count; i++) {
        if (tasks[i].state == PICO_RTOS_TASK_STATE_BLOCKED) {
            blocked_count++;
            printf("Task %s blocked for %lu ms\n", 
                   tasks[i].name, 
                   pico_rtos_get_tick_count() - tasks[i].last_run_time);
        }
    }
    
    if (blocked_count > task_count / 2) {
        printf("WARNING: Possible deadlock - %lu/%lu tasks blocked\n",
               blocked_count, task_count);
    }
}
```

**Solutions**:
1. **Implement Lock Ordering**
   ```c
   // Always acquire locks in the same order
   void acquire_multiple_locks(pico_rtos_mutex_t *lock1, pico_rtos_mutex_t *lock2) {
       pico_rtos_mutex_t *first = (lock1 < lock2) ? lock1 : lock2;
       pico_rtos_mutex_t *second = (lock1 < lock2) ? lock2 : lock1;
       
       pico_rtos_mutex_lock(first, 1000);
       pico_rtos_mutex_lock(second, 1000);
       
       // Use resources
       
       pico_rtos_mutex_unlock(second);
       pico_rtos_mutex_unlock(first);
   }
   ```

2. **Use Timeouts**
   ```c
   // Always use timeouts to prevent indefinite blocking
   if (!pico_rtos_mutex_lock(&mutex, 5000)) {  // 5 second timeout
       printf("ERROR: Mutex lock timeout\n");
       // Handle timeout appropriately
   }
   ```

### Priority Inversion

**Symptoms**: High-priority tasks delayed by low-priority tasks

**Diagnosis**:
```c
void detect_priority_inversion(void) {
    pico_rtos_task_info_t high_priority_task;
    pico_rtos_get_task_info(&critical_task, &high_priority_task);
    
    if (high_priority_task.state == PICO_RTOS_TASK_STATE_BLOCKED) {
        printf("WARNING: High-priority task blocked\n");
        // Check what it's waiting for
    }
}
```

**Solutions**:
```c
// Use priority inheritance mutexes (enabled by default in v0.3.1)
pico_rtos_mutex_t priority_mutex;
pico_rtos_mutex_init(&priority_mutex);  // Priority inheritance enabled

// Minimize critical section duration
void optimized_critical_section(void) {
    // Prepare data outside critical section
    data_t prepared_data = prepare_data();
    
    pico_rtos_mutex_lock(&mutex, 1000);
    shared_data = prepared_data;  // Quick operation
    pico_rtos_mutex_unlock(&mutex);
}
```

## Performance Issues

### High CPU Utilization

**Symptoms**: System sluggish, tasks missing deadlines

**Diagnosis**:
```c
void diagnose_cpu_usage(void) {
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    
    uint32_t cpu_usage = 100 - (stats.idle_counter * 100 / pico_rtos_get_uptime_ms());
    printf("CPU utilization: %lu%%\n", cpu_usage);
    
    if (cpu_usage > 80) {
        printf("WARNING: High CPU utilization\n");
        
        // Check context switch rate
        uint32_t switches_per_sec = stats.context_switches * 1000 / pico_rtos_get_uptime_ms();
        printf("Context switches: %lu/sec\n", switches_per_sec);
    }
}
```

**Solutions**:
1. **Optimize Task Delays**
   ```c
   // Avoid busy waiting
   void bad_task(void *param) {
       while (1) {
           if (condition_met()) {
               process_data();
           }
           // No delay - consumes CPU continuously
       }
   }
   
   void good_task(void *param) {
       while (1) {
           if (condition_met()) {
               process_data();
           }
           pico_rtos_task_delay(10);  // Give other tasks a chance
       }
   }
   ```

2. **Use Event-Driven Design**
   ```c
   // Replace polling with event-driven approach
   void event_driven_task(void *param) {
       while (1) {
           uint32_t events = pico_rtos_event_group_wait_bits(&events,
                                                            EVENT_DATA_READY,
                                                            false, true, 1000);
           if (events & EVENT_DATA_READY) {
               process_data();
           }
       }
   }
   ```

### Memory Leaks

**Symptoms**: Gradually increasing memory usage, eventual allocation failures

**Diagnosis**:
```c
void detect_memory_leaks(void) {
    static uint32_t last_memory_usage = 0;
    
    uint32_t current, peak, allocations;
    pico_rtos_get_memory_stats(&current, &peak, &allocations);
    
    if (current > last_memory_usage + 1024) {  // 1KB increase
        printf("WARNING: Memory usage increased by %lu bytes\n",
               current - last_memory_usage);
    }
    
    last_memory_usage = current;
}
```

**Solutions**:
1. **Use Memory Pools**
   ```c
   // Replace malloc/free with memory pools to prevent leaks
   void *buffer = pico_rtos_memory_pool_alloc(&pool, 1000);
   if (buffer) {
       // Use buffer
       pico_rtos_memory_pool_free(&pool, buffer);  // Always free
   }
   ```

2. **Implement Memory Tracking**
   ```c
   #ifdef DEBUG
   static uint32_t allocation_count = 0;
   static uint32_t deallocation_count = 0;
   
   void *debug_malloc(size_t size) {
       allocation_count++;
       return pico_rtos_malloc(size);
   }
   
   void debug_free(void *ptr, size_t size) {
       deallocation_count++;
       pico_rtos_free(ptr, size);
   }
   
   void check_memory_balance(void) {
       if (allocation_count != deallocation_count) {
           printf("WARNING: Memory imbalance - %lu allocs, %lu frees\n",
                  allocation_count, deallocation_count);
       }
   }
   #endif
   ```

## Feature-Specific Issues

### Event Groups Not Working

**Symptoms**: Tasks not waking up on events, incorrect event combinations

**Common Issues**:
1. **Incorrect Wait Semantics**
   ```c
   // Wrong: Using OR when you need AND
   uint32_t events = pico_rtos_event_group_wait_bits(&group,
                                                     EVENT_A | EVENT_B,
                                                     false,  // Wait for ANY (wrong)
                                                     true, 5000);
   
   // Correct: Using AND semantics
   uint32_t events = pico_rtos_event_group_wait_bits(&group,
                                                     EVENT_A | EVENT_B,
                                                     true,   // Wait for ALL (correct)
                                                     true, 5000);
   ```

2. **Event Bit Conflicts**
   ```c
   // Ensure event bits don't overlap
   #define EVENT_SENSOR_1    (1 << 0)
   #define EVENT_SENSOR_2    (1 << 1)
   #define EVENT_NETWORK     (1 << 2)
   // Don't reuse bit positions
   ```

### Stream Buffers Blocking

**Symptoms**: Stream buffer operations timeout, data not flowing

**Solutions**:
1. **Check Buffer Sizes**
   ```c
   // Ensure buffer is large enough for expected data
   uint32_t available = pico_rtos_stream_buffer_space_available(&stream);
   if (available < message_size) {
       printf("WARNING: Stream buffer full\n");
       // Increase buffer size or implement flow control
   }
   ```

2. **Implement Flow Control**
   ```c
   void send_with_flow_control(pico_rtos_stream_buffer_t *stream,
                              const void *data, uint32_t length) {
       while (pico_rtos_stream_buffer_space_available(stream) < length) {
           pico_rtos_task_delay(10);  // Wait for space
       }
       pico_rtos_stream_buffer_send(stream, data, length, 1000);
   }
   ```

### Memory Pool Exhaustion

**Symptoms**: Memory pool allocation failures despite available system memory

**Solutions**:
1. **Monitor Pool Usage**
   ```c
   void monitor_pool_usage(pico_rtos_memory_pool_t *pool) {
       pico_rtos_memory_pool_stats_t stats;
       if (pico_rtos_memory_pool_get_stats(pool, &stats)) {
           float usage = (float)stats.blocks_allocated / stats.total_blocks * 100.0f;
           if (usage > 80.0f) {
               printf("WARNING: Memory pool %.1f%% full\n", usage);
           }
       }
   }
   ```

2. **Implement Pool Expansion**
   ```c
   // Create multiple pools of different sizes
   pico_rtos_memory_pool_t small_pool, large_pool;
   
   void *smart_alloc(size_t size) {
       if (size <= 64) {
           void *ptr = pico_rtos_memory_pool_alloc(&small_pool, 100);
           if (ptr) return ptr;
       }
       
       if (size <= 256) {
           return pico_rtos_memory_pool_alloc(&large_pool, 100);
       }
       
       return NULL;  // Size too large
   }
   ```

## Debugging Tools

### Enable Debug Output

```c
// Enable comprehensive debug output
#define PICO_RTOS_DEBUG_LEVEL 3

// Enable specific subsystem debugging
#define PICO_RTOS_DEBUG_SCHEDULER 1
#define PICO_RTOS_DEBUG_MEMORY 1
#define PICO_RTOS_DEBUG_IPC 1
```

### Use Profiling Tools

```c
void setup_debugging(void) {
    // Initialize profiling
    pico_rtos_profiler_init(100);
    pico_rtos_profiler_start();
    
    // Initialize tracing
    pico_rtos_trace_init(1000);
    pico_rtos_trace_start();
    
    // Set up periodic health checks
    pico_rtos_task_create(&debug_task, "Debug", debug_monitor_task, NULL, 1024, 1);
}

void debug_monitor_task(void *param) {
    while (1) {
        detect_deadlocks();
        diagnose_memory_issues();
        diagnose_cpu_usage();
        
        pico_rtos_task_delay(5000);  // Check every 5 seconds
    }
}
```

### System Health Monitoring

```c
void system_health_check(void) {
    printf("\n=== SYSTEM HEALTH CHECK ===\n");
    
    // Check task states
    pico_rtos_task_info_t tasks[32];
    uint32_t task_count = pico_rtos_get_all_task_info(tasks, 32);
    
    uint32_t ready = 0, blocked = 0, suspended = 0;
    for (uint32_t i = 0; i < task_count; i++) {
        switch (tasks[i].state) {
            case PICO_RTOS_TASK_STATE_READY: ready++; break;
            case PICO_RTOS_TASK_STATE_BLOCKED: blocked++; break;
            case PICO_RTOS_TASK_STATE_SUSPENDED: suspended++; break;
        }
    }
    
    printf("Tasks: %lu total, %lu ready, %lu blocked, %lu suspended\n",
           task_count, ready, blocked, suspended);
    
    // Check memory
    uint32_t current, peak, allocations;
    pico_rtos_get_memory_stats(&current, &peak, &allocations);
    printf("Memory: %lu current, %lu peak (%.1f%% of peak)\n",
           current, peak, (float)current / peak * 100.0f);
    
    // Check system stats
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    printf("Context switches: %lu, Uptime: %lu ms\n",
           stats.context_switches, pico_rtos_get_uptime_ms());
    
    printf("===========================\n\n");
}
```

## Getting Help

### Information to Collect

When reporting issues, collect this information:

```c
void collect_debug_info(void) {
    printf("=== DEBUG INFORMATION ===\n");
    printf("Pico-RTOS Version: %s\n", pico_rtos_get_version_string());
    printf("Tick Rate: %lu Hz\n", pico_rtos_get_tick_rate_hz());
    printf("Uptime: %lu ms\n", pico_rtos_get_uptime_ms());
    
    // System configuration
    printf("Configuration:\n");
    printf("  Max Tasks: %d\n", PICO_RTOS_MAX_TASKS);
    printf("  Multi-core: %s\n", PICO_RTOS_ENABLE_MULTICORE ? "ON" : "OFF");
    printf("  Event Groups: %s\n", PICO_RTOS_ENABLE_EVENT_GROUPS ? "ON" : "OFF");
    
    // Hardware information
    printf("Hardware: RP2040\n");
    printf("Current Core: %lu\n", pico_rtos_get_current_core());
    
    // System health
    system_health_check();
}
```

### Common Support Channels

1. **GitHub Issues**: For bugs and feature requests
2. **Documentation**: Check the comprehensive guides
3. **Examples**: Review the provided example applications
4. **Community Forums**: For general questions and discussions

Remember to always include the debug information when seeking help, and provide a minimal reproducible example when possible.