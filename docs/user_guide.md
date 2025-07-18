# Pico-RTOS User Guide

This user guide provides an overview of the Pico-RTOS features and how to use them in your projects.

## Introduction

Pico-RTOS is a production-ready, lightweight real-time operating system specifically designed for the Raspberry Pi Pico board. Version 0.2.0 provides comprehensive features including:

- Complete ARM Cortex-M0+ context switching
- Priority inheritance to prevent priority inversion
- Stack overflow protection with dual canaries
- Memory management with tracking and leak detection
- Real-time system monitoring and diagnostics
- Thread-safe operations throughout

## Getting Started

Before using any RTOS functions, you must initialize the system:

```c
#include "pico_rtos.h"

int main() {
    // Initialize RTOS
    if (!pico_rtos_init()) {
        // Handle initialization failure
        return -1;
    }
    
    // Create your tasks here
    
    // Start the scheduler (this function does not return)
    pico_rtos_start();
    
    return 0;
}
```

## Tasks

Tasks are the basic units of execution in Pico-RTOS. Each task represents an independent thread of control with its own stack and priority.

To create a task, use the `pico_rtos_task_create` function:

```c
pico_rtos_task_t task;
pico_rtos_task_create(&task, "Task Name", task_function, NULL, stack_size, priority);
```

The task function should have the following signature:

```c
void task_function(void *param)
```

## Queues

Queues provide a mechanism for inter-task communication and data exchange. Tasks can send and receive messages through queues.

To create a queue, use the `pico_rtos_queue_init` function:

```c
pico_rtos_queue_t queue;
uint8_t queue_buffer[QUEUE_SIZE];
pico_rtos_queue_init(&queue, queue_buffer, sizeof(uint8_t), QUEUE_SIZE);
```

To send and receive messages, use the `pico_rtos_queue_send` and `pico_rtos_queue_receive` functions:

```c
uint8_t item = 42;
pico_rtos_queue_send(&queue, &item, 0);

uint8_t received_item;
pico_rtos_queue_receive(&queue, &received_item, PICO_RTOS_WAIT_FOREVER);
```

## Semaphores

Semaphores provide a mechanism for synchronization and resource management between tasks.

To create a semaphore, use the `pico_rtos_semaphore_init` function:

```c
pico_rtos_semaphore_t semaphore;
pico_rtos_semaphore_init(&semaphore, initial_count, max_count);
```

To acquire and release a semaphore, use the `pico_rtos_semaphore_take` and `pico_rtos_semaphore_give` functions:

```c
pico_rtos_semaphore_take(&semaphore, PICO_RTOS_WAIT_FOREVER);
// Critical section
pico_rtos_semaphore_give(&semaphore);
```

## Mutexes

Mutexes provide mutual exclusion and synchronization between tasks accessing shared resources.

To create a mutex, use the `pico_rtos_mutex_init` function:

```c
pico_rtos_mutex_t mutex;
pico_rtos_mutex_init(&mutex);
```

To acquire and release a mutex, use the `pico_rtos_mutex_lock` and `pico_rtos_mutex_unlock` functions:

```c
pico_rtos_mutex_lock(&mutex);
// Critical section
pico_rtos_mutex_unlock(&mutex);
```

## Timers

Timers allow you to schedule periodic or one-shot callbacks.

To create a timer, use the `pico_rtos_timer_init` function:

```c
pico_rtos_timer_t timer;
pico_rtos_timer_init(&timer, "Timer", timer_callback, NULL, period, auto_reload);
```

To start and stop a timer, use the `pico_rtos_timer_start` and `pico_rtos_timer_stop` functions:

```c
pico_rtos_timer_start(&timer);
// Timer running
pico_rtos_timer_stop(&timer);
```

The timer callback function should have the following signature:

```c
void timer_callback(void *param)
```

For more detailed information on the Pico-RTOS API, refer to the [API Reference](api_reference.md).

## Best Practices

- Assign appropriate priorities to tasks based on their importance and real-time requirements.
- Use mutexes and semaphores judiciously to avoid deadlocks and priority inversion.
- Keep critical sections as short as possible to minimize blocking time.
- Use queues for inter-task communication instead of shared memory wherever possible.
- Handle timer callbacks efficiently and avoid performing time-consuming operations within them.

## Conclusion

Pico-RTOS provides a simple and efficient way to develop real-time applications on the Raspberry Pi Pico board. By leveraging its features and following best practices, you can create robust and responsive embedded systems.

For more information and examples, refer to the [Examples](examples/) directory in the Pico-RTOS repository.

Happy coding with Pico-RTOS!

---
## Pri
ority Management

Pico-RTOS uses priority-based preemptive scheduling. Higher priority tasks preempt lower priority tasks. Tasks with the same priority use round-robin scheduling.

### Priority Inheritance

Mutexes support priority inheritance to prevent priority inversion. When a high-priority task blocks on a mutex owned by a lower-priority task, the owner's priority is temporarily boosted.

```c
// High priority task (priority 5)
void high_priority_task(void *param) {
    while (1) {
        if (pico_rtos_mutex_lock(&shared_mutex, 1000)) {
            // Critical section
            pico_rtos_mutex_unlock(&shared_mutex);
        }
        pico_rtos_task_delay(100);
    }
}

// Low priority task (priority 1) - will be boosted when high priority task waits
void low_priority_task(void *param) {
    while (1) {
        if (pico_rtos_mutex_lock(&shared_mutex, PICO_RTOS_WAIT_FOREVER)) {
            // Long critical section - priority will be boosted
            pico_rtos_task_delay(500);
            pico_rtos_mutex_unlock(&shared_mutex);
        }
        pico_rtos_task_delay(1000);
    }
}
```

### Dynamic Priority Changes

You can change task priorities at runtime:

```c
// Change current task priority
pico_rtos_task_set_priority(NULL, 3);

// Change another task's priority
pico_rtos_task_set_priority(&my_task, 5);
```

## Memory Management

Pico-RTOS provides tracked memory allocation to prevent leaks and monitor usage:

```c
// Use tracked allocation instead of malloc
void *buffer = pico_rtos_malloc(1024);

// Free with size tracking
pico_rtos_free(buffer, 1024);

// Get memory statistics
uint32_t current, peak, allocations;
pico_rtos_get_memory_stats(&current, &peak, &allocations);
printf("Memory: %lu current, %lu peak, %lu allocations\n", 
       current, peak, allocations);
```

## Stack Overflow Protection

All tasks are automatically protected with stack canaries:

```c
// Stack overflow is detected automatically
void task_with_deep_recursion(void *param) {
    char large_buffer[2048];  // This might cause overflow
    // ... stack overflow will be detected and handled
}

// Custom stack overflow handler (optional)
void pico_rtos_handle_stack_overflow(pico_rtos_task_t *task) {
    printf("Stack overflow in task: %s\n", task->name);
    // Custom handling logic
    pico_rtos_task_delete(task);  // Terminate the task
}
```

## System Monitoring

Get comprehensive system statistics:

```c
void monitor_task(void *param) {
    pico_rtos_system_stats_t stats;
    
    while (1) {
        pico_rtos_get_system_stats(&stats);
        
        printf("System Stats:\n");
        printf("  Tasks: %lu total, %lu ready, %lu blocked\n",
               stats.total_tasks, stats.ready_tasks, stats.blocked_tasks);
        printf("  Memory: %lu current, %lu peak\n",
               stats.current_memory, stats.peak_memory);
        printf("  Uptime: %lu ms\n", stats.system_uptime);
        
        // Calculate CPU usage (approximate)
        uint32_t idle_count = pico_rtos_get_idle_counter();
        // CPU usage calculation based on idle counter
        
        pico_rtos_task_delay(5000);  // Report every 5 seconds
    }
}
```

## Advanced Features

### Interrupt-Safe Operations

Use interrupt-safe context switching:

```c
void my_interrupt_handler(void) {
    pico_rtos_interrupt_enter();
    
    // Interrupt handling code
    
    // Request context switch if needed
    pico_rtos_request_context_switch();
    
    pico_rtos_interrupt_exit();
}
```

### Task Notifications

Use semaphores for lightweight task notifications:

```c
pico_rtos_semaphore_t notification;
pico_rtos_semaphore_init(&notification, 0, 1);  // Binary semaphore

// Notifying task
void sender_task(void *param) {
    while (1) {
        // Do work
        pico_rtos_semaphore_give(&notification);  // Notify
        pico_rtos_task_delay(1000);
    }
}

// Waiting task
void receiver_task(void *param) {
    while (1) {
        if (pico_rtos_semaphore_take(&notification, PICO_RTOS_WAIT_FOREVER)) {
            // Handle notification
            printf("Notification received!\n");
        }
    }
}
```

## Best Practices

### Task Design
- Keep task functions simple and focused
- Use appropriate stack sizes (minimum 128 bytes)
- Set priorities based on real-time requirements
- Use delays to prevent CPU hogging

### Synchronization
- Always use timeouts for blocking operations
- Prefer mutexes for mutual exclusion
- Use semaphores for signaling and counting
- Keep critical sections short

### Memory Management
- Use tracked allocation functions
- Monitor memory usage regularly
- Free resources in task cleanup
- Validate allocation success

### Error Handling
- Check return values of all RTOS functions
- Implement proper error recovery
- Use system statistics for debugging
- Monitor stack usage

## Performance Considerations

- Context switch time: ~50-100 CPU cycles
- Memory overhead: ~32 bytes per task + stack
- Interrupt latency: Minimal with proper nesting
- Timer resolution: 1ms system tick

## Debugging Tips

1. **Use System Statistics**: Monitor task states and memory usage
2. **Check Stack Usage**: Enable stack overflow protection
3. **Monitor Priorities**: Ensure proper priority assignment
4. **Validate Timeouts**: Use appropriate timeout values
5. **Test Edge Cases**: Test with maximum loads and edge conditions

## Migration from Other RTOSes

If migrating from FreeRTOS or other RTOSes:

- Task creation is similar but requires explicit stack size
- Priority inheritance is automatic in mutexes
- Memory management is tracked by default
- System monitoring is built-in
- Stack overflow protection is automatic

## Conclusion

Pico-RTOS v0.2.0 provides a production-ready RTOS with comprehensive features for embedded development on Raspberry Pi Pico. The combination of real-time capabilities, safety features, and monitoring tools makes it suitable for professional embedded applications.