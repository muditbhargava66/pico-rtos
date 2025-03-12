# Troubleshooting Guide for Pico-RTOS

This guide provides solutions to common issues you might encounter when working with Pico-RTOS.

## Build Issues

### CMake Cannot Find Pico SDK

**Symptom:** CMake fails with an error about not finding the Pico SDK.

**Solution:**
1. Ensure you've set the `PICO_SDK_PATH` environment variable:
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```
2. Or provide the path when running CMake:
   ```bash
   cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
   ```

### Build Fails with Missing Header Files

**Symptom:** Compilation fails with errors about missing header files.

**Solution:**
1. Check that your include paths are correct.
2. Ensure you've included the correct header files in your code.
3. Verify that your project directory structure matches the expected layout.

## Runtime Issues

### Tasks Not Running

**Symptom:** Some tasks aren't executing as expected.

**Solutions:**
1. Check task priorities. Higher priority tasks might be preventing lower priority tasks from running.
2. Ensure tasks aren't stuck in infinite loops without yielding or delaying.
3. Verify that the task stack size is sufficient. If a task overflows its stack, it might crash.
4. Check if tasks are properly initialized and added to the scheduler.

### Deadlocks

**Symptom:** The system freezes or multiple tasks become unresponsive.

**Solutions:**
1. Check mutex acquisition order. Acquiring mutexes in different orders can lead to deadlocks.
2. Ensure you're releasing all mutexes and semaphores properly.
3. Verify timeout values for resource acquisition.
4. Check for priority inversion scenarios and consider using priority inheritance.

### Memory Issues

**Symptom:** System crashes, resets, or behaves unpredictably.

**Solutions:**
1. Increase task stack sizes if stack overflow is suspected.
2. Check for memory leaks or improper memory management.
3. Verify critical section handling to ensure proper resource protection.
4. Use debugging tools to track memory usage.

## Debugging Tips

### Using printf for Debugging

You can add printf statements to your code to trace execution and debug issues:

```c
printf("Task %s: state=%s, priority=%u\n", 
       task->name, 
       pico_rtos_task_get_state_string(task),
       task->priority);
```

### Checking Task States

Monitor task states to understand what's happening in your system:

```c
void debug_print_tasks(void) {
    pico_rtos_task_t *task = task_list;  // Assuming task_list is accessible
    
    printf("--- Task Status ---\n");
    while (task != NULL) {
        printf("Task: %s, State: %s, Priority: %u\n",
               task->name,
               pico_rtos_task_get_state_string(task),
               task->priority);
        task = task->next;
    }
    printf("------------------\n");
}
```

### Timing Issues

For timing-related problems:

1. Use the `pico_rtos_get_tick_count()` function to measure time intervals.
2. Check that the system timer is correctly configured.
3. Verify that task delays are working as expected.

## Advanced Issues

### Interrupt Handling

If you're having issues with interrupts:

1. Ensure interrupts are properly enabled/disabled in critical sections.
2. Check priority levels for interrupts.
3. Keep interrupt service routines (ISRs) short and efficient.

### Resource Contention

For resource contention issues:

1. Use mutexes for exclusive access to resources.
2. Consider using reader-writer locks for resources with multiple readers.
3. Monitor resource usage patterns and optimize access.

## Getting Help

If you continue to experience issues:

1. Check the example projects in the `examples/` directory.
2. Review the API documentation for proper usage.
3. Open an issue on the GitHub repository with a detailed description of your problem.

---