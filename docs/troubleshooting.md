# Troubleshooting Guide

This guide helps you diagnose and resolve common issues when using Pico-RTOS.

## Build Issues

### CMake Configuration Errors

**Problem**: CMake fails to find the Pico SDK
```
CMake Error: Could not find Pico SDK
```

**Solution**:
1. Ensure the Pico SDK submodule is initialized:
   ```bash
   git submodule update --init --recursive
   ```
2. Or set the SDK path manually:
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```

**Problem**: Missing ARM toolchain
```
CMake Error: Could not find arm-none-eabi-gcc
```

**Solution**:
1. Install the ARM toolchain:
   ```bash
   # macOS
   brew install arm-none-eabi-gcc
   
   # Ubuntu/Debian
   sudo apt-get install gcc-arm-none-eabi
   ```

### Compilation Errors

**Problem**: Missing function declarations
```
warning: implicit declaration of function 'pico_rtos_init'
```

**Solution**:
1. Include the main header:
   ```c
   #include "pico_rtos.h"
   ```
2. Ensure proper linking in CMakeLists.txt:
   ```cmake
   target_link_libraries(your_target pico_rtos)
   ```

**Problem**: Stack size too small
```
error: stack size must be at least 128 bytes
```

**Solution**:
```c
// Increase stack size
pico_rtos_task_create(&task, "Task", task_func, NULL, 512, 1);  // 512 bytes minimum
```

## Runtime Issues

### System Doesn't Start

**Problem**: RTOS doesn't start or hangs during initialization

**Debugging Steps**:
1. Check initialization return value:
   ```c
   if (!pico_rtos_init()) {
       printf("RTOS initialization failed\n");
       return -1;
   }
   ```

2. Verify at least one task is created before starting:
   ```c
   // Must create at least one task
   pico_rtos_task_create(&task, "Main", main_task, NULL, 256, 1);
   pico_rtos_start();  // This should not return
   ```

3. Check for stack overflow in early tasks:
   ```c
   // Use larger stack sizes during debugging
   pico_rtos_task_create(&task, "Debug", task_func, NULL, 1024, 1);
   ```

### Task Scheduling Issues

**Problem**: High priority task doesn't run

**Debugging**:
1. Check task states:
   ```c
   printf("Task state: %s\n", pico_rtos_task_get_state_string(&task));
   ```

2. Verify priority values (higher number = higher priority):
   ```c
   pico_rtos_task_create(&high_task, "High", func, NULL, 256, 5);  // High priority
   pico_rtos_task_create(&low_task, "Low", func, NULL, 256, 1);   // Low priority
   ```

3. Check for blocking operations:
   ```c
   // Avoid infinite loops without delays
   void task_func(void *param) {
       while (1) {
           // Do work
           pico_rtos_task_delay(10);  // Allow other tasks to run
       }
   }
   ```

**Problem**: Tasks appear to hang

**Solution**:
1. Check for deadlocks:
   ```c
   // Always use timeouts
   if (pico_rtos_mutex_lock(&mutex, 1000)) {  // 1 second timeout
       // Critical section
       pico_rtos_mutex_unlock(&mutex);
   } else {
       printf("Mutex timeout - possible deadlock\n");
   }
   ```

2. Monitor system statistics:
   ```c
   pico_rtos_system_stats_t stats;
   pico_rtos_get_system_stats(&stats);
   printf("Ready tasks: %lu, Blocked tasks: %lu\n", 
          stats.ready_tasks, stats.blocked_tasks);
   ```

### Memory Issues

**Problem**: System runs out of memory

**Debugging**:
1. Monitor memory usage:
   ```c
   uint32_t current, peak, allocations;
   pico_rtos_get_memory_stats(&current, &peak, &allocations);
   printf("Memory: %lu/%lu bytes used\n", current, peak);
   ```

2. Check for memory leaks:
   ```c
   void *ptr = pico_rtos_malloc(100);
   // ... use ptr
   pico_rtos_free(ptr, 100);  // Don't forget to free!
   ```

3. Reduce stack sizes if possible:
   ```c
   // Use minimum required stack size
   pico_rtos_task_create(&task, "Small", func, NULL, 256, 1);
   ```

**Problem**: Stack overflow detected

**Solution**:
1. Increase stack size:
   ```c
   pico_rtos_task_create(&task, "Task", func, NULL, 1024, 1);  // Larger stack
   ```

2. Reduce local variable usage:
   ```c
   void task_func(void *param) {
       // Avoid large local arrays
       // char big_buffer[2048];  // This uses stack space
       
       // Use dynamic allocation instead
       char *buffer = pico_rtos_malloc(2048);
       // ... use buffer
       pico_rtos_free(buffer, 2048);
   }
   ```

3. Implement custom stack overflow handler:
   ```c
   void pico_rtos_handle_stack_overflow(pico_rtos_task_t *task) {
       printf("Stack overflow in task: %s\n", task->name);
       // Log additional information
       pico_rtos_task_delete(task);  // Clean up
   }
   ```

## Communication Issues

### Serial Output Problems

**Problem**: No serial output visible

**Solutions**:
1. Check baud rate (should be 115200):
   ```bash
   screen /dev/tty.usbmodem14101 115200
   ```

2. Ensure `stdio_init_all()` is called:
   ```c
   int main() {
       stdio_init_all();  // Initialize USB CDC
       // ... rest of code
   }
   ```

3. Try different terminal programs:
   ```bash
   # Try minicom
   minicom -D /dev/tty.usbmodem14101 -b 115200
   
   # Or cu
   cu -l /dev/tty.usbmodem14101 -s 115200
   ```

4. Check USB cable (must support data, not just power)

**Problem**: Garbled or partial output

**Solutions**:
1. Add delays between prints:
   ```c
   printf("Message 1\n");
   pico_rtos_task_delay(10);  // Small delay
   printf("Message 2\n");
   ```

2. Use critical sections for multi-task printing:
   ```c
   pico_rtos_mutex_t print_mutex;
   
   void safe_printf(const char *format, ...) {
       pico_rtos_mutex_lock(&print_mutex, PICO_RTOS_WAIT_FOREVER);
       va_list args;
       va_start(args, format);
       vprintf(format, args);
       va_end(args);
       pico_rtos_mutex_unlock(&print_mutex);
   }
   ```

### Queue/Semaphore Issues

**Problem**: Queue operations fail

**Debugging**:
1. Check queue initialization:
   ```c
   uint8_t buffer[10 * sizeof(int)];  // Buffer for 10 integers
   if (!pico_rtos_queue_init(&queue, buffer, sizeof(int), 10)) {
       printf("Queue initialization failed\n");
   }
   ```

2. Use appropriate timeouts:
   ```c
   // For sending
   if (!pico_rtos_queue_send(&queue, &item, 1000)) {
       printf("Queue send timeout\n");
   }
   
   // For receiving
   if (!pico_rtos_queue_receive(&queue, &item, 1000)) {
       printf("Queue receive timeout\n");
   }
   ```

3. Check queue status:
   ```c
   if (pico_rtos_queue_is_full(&queue)) {
       printf("Queue is full\n");
   }
   if (pico_rtos_queue_is_empty(&queue)) {
       printf("Queue is empty\n");
   }
   ```

## Hardware Issues

### Pico Not Recognized

**Problem**: Computer doesn't detect the Pico

**Solutions**:
1. Try different USB cables
2. Try different USB ports
3. Check if Pico is in BOOTSEL mode (should appear as "RPI-RP2" drive)
4. Press reset button while holding BOOTSEL

**Problem**: Flashing fails

**Solutions**:
1. Ensure Pico is in BOOTSEL mode
2. Check .uf2 file is valid (should be generated by build)
3. Try copying file manually instead of drag-and-drop
4. Verify sufficient disk space on host system

## Performance Issues

### System Appears Slow

**Debugging**:
1. Check CPU usage via idle counter:
   ```c
   uint32_t idle_count = pico_rtos_get_idle_counter();
   // Lower values indicate higher CPU usage
   ```

2. Profile task execution times:
   ```c
   void task_func(void *param) {
       while (1) {
           uint32_t start = pico_rtos_get_tick_count();
           
           // Do work
           
           uint32_t end = pico_rtos_get_tick_count();
           printf("Task execution time: %lu ms\n", end - start);
           
           pico_rtos_task_delay(100);
       }
   }
   ```

3. Check for priority inversion:
   ```c
   // Monitor mutex ownership
   pico_rtos_task_t *owner = pico_rtos_mutex_get_owner(&mutex);
   if (owner) {
       printf("Mutex owned by: %s\n", owner->name);
   }
   ```

### High Interrupt Latency

**Solutions**:
1. Keep critical sections short:
   ```c
   pico_rtos_enter_critical();
   // Minimal code here
   pico_rtos_exit_critical();
   ```

2. Use interrupt-safe operations:
   ```c
   void interrupt_handler(void) {
       pico_rtos_interrupt_enter();
       
       // Handle interrupt
       
       pico_rtos_interrupt_exit();
   }
   ```

## Debugging Tools

### System Statistics

Use comprehensive system monitoring:

```c
void debug_system_state(void) {
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    
    printf("\n=== SYSTEM DEBUG INFO ===\n");
    printf("Tasks: %lu total, %lu ready, %lu blocked, %lu suspended\n",
           stats.total_tasks, stats.ready_tasks, 
           stats.blocked_tasks, stats.suspended_tasks);
    printf("Memory: %lu current, %lu peak, %lu allocations\n",
           stats.current_memory, stats.peak_memory, stats.total_allocations);
    printf("Uptime: %lu ms, Idle: %lu\n",
           stats.system_uptime, stats.idle_counter);
    printf("========================\n\n");
}
```

### Task State Monitoring

```c
void debug_task_state(pico_rtos_task_t *task) {
    printf("Task: %s\n", task->name ? task->name : "Unknown");
    printf("State: %s\n", pico_rtos_task_get_state_string(task));
    printf("Priority: %lu\n", task->priority);
    printf("Stack: %p (size: %lu)\n", task->stack_base, task->stack_size);
}
```

### Memory Leak Detection

```c
void check_memory_leaks(void) {
    static uint32_t last_allocations = 0;
    
    uint32_t current, peak, allocations;
    pico_rtos_get_memory_stats(&current, &peak, &allocations);
    
    if (allocations > last_allocations) {
        printf("Memory allocations increased: %lu -> %lu\n",
               last_allocations, allocations);
        last_allocations = allocations;
    }
    
    if (current > 0) {
        printf("Warning: %lu bytes still allocated\n", current);
    }
}
```

## Getting Help

If you're still experiencing issues:

1. **Check the examples** in the `examples/` directory
2. **Review the API documentation** in `docs/api_reference.md`
3. **Enable debug output** and monitor system statistics
4. **Create a minimal test case** that reproduces the issue
5. **Check the GitHub issues** for similar problems
6. **Open a new issue** with detailed information:
   - Pico-RTOS version
   - Build environment details
   - Complete error messages
   - Minimal code example
   - Steps to reproduce

## Common Gotchas

1. **Forgetting to call `stdio_init_all()`** - No serial output
2. **Using insufficient stack sizes** - Random crashes
3. **Not using timeouts** - Tasks hang indefinitely
4. **Forgetting to free memory** - Memory leaks
5. **Priority inversion** - High priority tasks blocked
6. **Infinite loops without delays** - System appears frozen
7. **Wrong baud rate** - Garbled serial output
8. **USB cable doesn't support data** - No communication

Remember: When in doubt, start with the working examples and modify them incrementally to understand the behavior.