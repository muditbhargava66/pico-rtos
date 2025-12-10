# Pico-RTOS Troubleshooting Guide

This guide helps you diagnose and resolve common issues with Pico-RTOS across all versions.

**Current Version**: v0.3.1 "Advanced Synchronization & Multi-Core"

## 🔍 Quick Diagnosis

### System Information
```c
// Get system information for troubleshooting
printf("Pico-RTOS Version: %s\n", pico_rtos_get_version_string());
printf("Tick Rate: %lu Hz\n", pico_rtos_get_tick_rate_hz());
printf("Uptime: %lu ms\n", pico_rtos_get_uptime_ms());

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
printf("Multi-core: Enabled\n");
printf("Current Core: %d\n", get_core_num());
#endif
```

### Health Check *(v0.3.1)*
```c
// Quick system health check
pico_rtos_system_stats_t stats;
if (pico_rtos_get_system_stats(&stats)) {
    printf("CPU Usage: %lu%%\n", stats.cpu_usage_percent);
    printf("Memory Used: %lu/%lu bytes\n", stats.memory_used, stats.memory_total);
    printf("Tasks Running: %lu\n", stats.tasks_running);
}
```

---

## 🚨 Common Issues

### Build and Configuration Issues

#### Issue: Build Fails After Updating to v0.3.1
**Symptoms:**
```
error: 'PICO_RTOS_ENABLE_EVENT_GROUPS' undeclared
fatal error: 'pico_rtos_config.h' file not found
```

**Solution:**
```bash
# Regenerate configuration files
make clean
python3 scripts/menuconfig.py --generate
make build

# Or use the migration tool
python3 scripts/menuconfig.py --migrate-from-v0.2.1
```

#### Issue: Menuconfig Not Working
**Symptoms:**
```
ModuleNotFoundError: No module named 'kconfiglib'
```

**Solution:**
```bash
# Install required dependencies
pip3 install kconfiglib

# Or use the automated installer
make install-deps
```

#### Issue: Configuration Changes Not Taking Effect
**Symptoms:**
- New features not available despite being enabled
- Build uses old configuration

**Solution:**
```bash
# Check configuration generation
cat cmake_config.cmake | grep ENABLE_EVENT_GROUPS

# Force regeneration
rm cmake_config.cmake include/pico_rtos_config.h
make configure

# Verify configuration
make showconfig
```

### Runtime Issues

#### Issue: System Hangs or Freezes
**Symptoms:**
- System stops responding
- No output after initialization
- Watchdog resets (if enabled)

**Diagnosis:**
```c
// Add debug output to identify hang location
void debug_task_states() {
    printf("=== Task States ===\n");
    
    #ifdef PICO_RTOS_ENABLE_TASK_INSPECTION
    // Check all task states
    for (int i = 0; i < PICO_RTOS_MAX_TASKS; i++) {
        pico_rtos_task_info_t info;
        if (pico_rtos_debug_get_task_info_by_index(i, &info)) {
            printf("Task %s: %s\n", info.name, task_state_to_string(info.state));
        }
    }
    #endif
}
```

**Common Causes & Solutions:**
1. **Deadlock**: Use deadlock detection *(v0.3.1)*
   ```cmake
   set(PICO_RTOS_ENABLE_DEADLOCK_DETECTION ON)
   ```

2. **Stack Overflow**: Enable stack checking
   ```cmake
   set(PICO_RTOS_ENABLE_STACK_CHECKING ON)
   ```

3. **Infinite Loop in ISR**: Keep ISRs minimal
   ```c
   void __isr timer_isr() {
       // Clear interrupt FIRST
       timer_hw->intr = 1;
       
       // Minimal processing only
       pico_rtos_semaphore_give_from_isr(&timer_semaphore);
   }
   ```

#### Issue: Poor Performance or High CPU Usage
**Symptoms:**
- System feels sluggish
- Tasks missing deadlines
- High CPU usage reported

**Diagnosis:**
```c
// Monitor system performance
void monitor_performance() {
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    
    if (stats.cpu_usage_percent > 80) {
        printf("WARNING: High CPU usage: %lu%%\n", stats.cpu_usage_percent);
        
        #ifdef PICO_RTOS_ENABLE_TASK_INSPECTION
        // Find CPU-intensive tasks
        analyze_task_cpu_usage();
        #endif
    }
}
```

**Solutions:**
1. **Optimize Task Priorities**: Ensure proper priority assignment
2. **Reduce Context Switching**: Batch work in tasks
3. **Use Appropriate Delays**: Avoid busy-waiting
4. **Enable Multi-Core**: Distribute load *(v0.3.1)*

#### Issue: Memory Issues
**Symptoms:**
- malloc() returns NULL
- Stack overflow errors
- System instability

**Diagnosis:**
```c
// Check memory usage
void check_memory_health() {
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    
    float usage = (float)stats.memory_used / stats.memory_total * 100;
    printf("Memory usage: %.1f%% (%lu/%lu bytes)\n", 
           usage, stats.memory_used, stats.memory_total);
    
    if (usage > 90) {
        printf("WARNING: Low memory!\n");
        
        #ifdef PICO_RTOS_ENABLE_MEMORY_TRACKING
        // Show memory allocation details
        show_memory_allocations();
        #endif
    }
}
```

**Solutions:**
1. **Reduce Stack Sizes**: Right-size task stacks
2. **Use Memory Pools**: For frequent allocations *(v0.3.1)*
3. **Fix Memory Leaks**: Track allocations
4. **Optimize Data Structures**: Reduce memory footprint

### Multi-Core Issues *(v0.3.1)*

#### Issue: Multi-Core Not Working
**Symptoms:**
- Tasks only running on one core
- No performance improvement
- SMP initialization fails

**Diagnosis:**
```c
// Check multi-core status
void check_multicore_status() {
    #ifdef PICO_RTOS_ENABLE_MULTI_CORE
    printf("Multi-core enabled\n");
    printf("Current core: %d\n", get_core_num());
    
    pico_rtos_system_stats_t stats;
    if (pico_rtos_get_system_stats(&stats)) {
        printf("Core 0 usage: %lu%%\n", stats.cpu_usage_core0);
        printf("Core 1 usage: %lu%%\n", stats.cpu_usage_core1);
    }
    #else
    printf("Multi-core disabled\n");
    #endif
}
```

**Solutions:**
1. **Verify Hardware**: Ensure RP2040 or compatible
2. **Check Configuration**: Enable multi-core features
3. **Initialize SMP**: Call `pico_rtos_smp_init()`
4. **Review Task Affinity**: Ensure tasks can migrate

#### Issue: Poor Multi-Core Performance
**Symptoms:**
- Uneven core utilization
- Frequent task migration
- Synchronization bottlenecks

**Solutions:**
1. **Adjust Load Balancing**: Tune threshold
   ```cmake
   set(PICO_RTOS_LOAD_BALANCE_THRESHOLD 80)  # Higher threshold
   ```

2. **Use Core Affinity**: Pin critical tasks
   ```c
   pico_rtos_task_set_core_affinity(&critical_task, 0x01);  // Core 0 only
   ```

3. **Optimize Synchronization**: Minimize shared resources
4. **Profile Performance**: Use execution profiling

### Synchronization Issues

#### Issue: Deadlock Detection *(v0.3.1)*
**Symptoms:**
- System hangs with multiple tasks blocked
- Deadlock detection alerts

**Diagnosis:**
```c
#ifdef PICO_RTOS_ENABLE_DEADLOCK_DETECTION
void check_deadlock_status() {
    pico_rtos_deadlock_info_t info;
    if (pico_rtos_deadlock_get_info(&info)) {
        if (info.potential_deadlock) {
            printf("DEADLOCK DETECTED!\n");
            printf("Involved tasks: %lu\n", info.involved_tasks);
            // Print deadlock details
        }
    }
}
#endif
```

**Solutions:**
1. **Consistent Lock Ordering**: Always acquire locks in same order
2. **Use Timeouts**: Avoid infinite waits
3. **Minimize Lock Scope**: Keep critical sections short
4. **Review Lock Dependencies**: Simplify synchronization

#### Issue: Event Groups Not Working *(v0.3.1)*
**Symptoms:**
- Tasks not waking up on events
- Event bits not being set/cleared properly

**Common Mistakes:**
```c
// WRONG: Not initializing event group
pico_rtos_event_group_t events;  // Uninitialized!
pico_rtos_event_group_set_bits(&events, 0x01);

// CORRECT: Initialize first
pico_rtos_event_group_t events;
pico_rtos_event_group_init(&events);
pico_rtos_event_group_set_bits(&events, 0x01);

// WRONG: Using wrong wait semantics
uint32_t result = pico_rtos_event_group_wait_bits(
    &events, 0x01 | 0x02, true, false, 1000);  // wait_all=true

// CORRECT: Use appropriate semantics
uint32_t result = pico_rtos_event_group_wait_bits(
    &events, 0x01 | 0x02, false, true, 1000);  // wait_any=false, clear=true
```

#### Issue: Stream Buffer Problems *(v0.3.1)*
**Symptoms:**
- Data corruption in stream buffers
- Send/receive operations failing
- Buffer overflow

**Common Issues:**
```c
// WRONG: Buffer too small
uint8_t small_buffer[64];
pico_rtos_stream_buffer_init(&stream, small_buffer, sizeof(small_buffer));

// Trying to send large message
uint8_t large_data[256];
pico_rtos_stream_buffer_send(&stream, large_data, sizeof(large_data), 1000);  // Fails!

// CORRECT: Size buffer appropriately
uint8_t adequate_buffer[1024];  // Size for expected data + overhead
pico_rtos_stream_buffer_init(&stream, adequate_buffer, sizeof(adequate_buffer));
```

---

## 🔧 Debugging Tools

### Built-in Debugging *(v0.3.1)*

#### Task Inspection
```c
void debug_all_tasks() {
    #ifdef PICO_RTOS_ENABLE_TASK_INSPECTION
    printf("\n=== Task Debug Information ===\n");
    
    for (int i = 0; i < PICO_RTOS_MAX_TASKS; i++) {
        pico_rtos_task_info_t info;
        if (pico_rtos_debug_get_task_info_by_index(i, &info)) {
            printf("Task: %s\n", info.name);
            printf("  State: %s\n", task_state_to_string(info.state));
            printf("  Priority: %lu\n", info.priority);
            printf("  Stack: %lu/%lu bytes (%.1f%% used)\n",
                   info.stack_used, info.stack_size,
                   (float)info.stack_used / info.stack_size * 100);
            
            if (info.stack_used > info.stack_size * 0.9) {
                printf("  WARNING: Stack usage high!\n");
            }
            
            #ifdef PICO_RTOS_ENABLE_MULTI_CORE
            printf("  Core Affinity: 0x%02X\n", info.core_affinity);
            printf("  Current Core: %lu\n", info.current_core);
            #endif
            printf("\n");
        }
    }
    #endif
}
```

#### System Tracing
```c
void enable_system_tracing() {
    #ifdef PICO_RTOS_ENABLE_SYSTEM_TRACING
    pico_rtos_trace_start();
    
    // Run your problematic code here
    problematic_function();
    
    pico_rtos_trace_stop();
    
    // Analyze trace
    pico_rtos_trace_entry_t trace_buffer[64];
    uint32_t count = 64;
    if (pico_rtos_trace_get_events(trace_buffer, &count)) {
        printf("Trace events: %lu\n", count);
        for (uint32_t i = 0; i < count; i++) {
            printf("  %lu: %s\n", trace_buffer[i].timestamp, 
                   trace_event_to_string(trace_buffer[i].event));
        }
    }
    #endif
}
```

### External Debugging Tools

#### GDB Debugging
```bash
# Build with debug symbols
make build BUILD_TYPE=Debug

# Start GDB session
arm-none-eabi-gdb build/your_program.elf

# Useful GDB commands for RTOS debugging
(gdb) info threads              # Show all tasks
(gdb) thread 2                  # Switch to task 2
(gdb) bt                        # Show call stack
(gdb) info registers            # Show CPU registers
```

#### Logic Analyzer
```c
// Add debug pins for logic analyzer
#define DEBUG_PIN_TASK_SWITCH 2
#define DEBUG_PIN_ISR_ENTRY   3

void debug_task_switch() {
    gpio_put(DEBUG_PIN_TASK_SWITCH, 1);
    // Brief pulse to indicate task switch
    gpio_put(DEBUG_PIN_TASK_SWITCH, 0);
}

void __isr debug_isr_entry() {
    gpio_put(DEBUG_PIN_ISR_ENTRY, 1);
    // Your ISR code
    gpio_put(DEBUG_PIN_ISR_ENTRY, 0);
}
```

---

## 📊 Diagnostic Procedures

### Performance Issues

#### Step 1: Baseline Measurement
```c
void measure_baseline_performance() {
    printf("=== Baseline Performance ===\n");
    
    // Measure context switch time
    uint32_t start = time_us_32();
    for (int i = 0; i < 1000; i++) {
        pico_rtos_task_yield();
    }
    uint32_t end = time_us_32();
    printf("Context switch: %.2f us\n", (end - start) / 1000.0);
    
    // Measure system overhead
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    printf("System overhead: %lu%%\n", stats.system_overhead_percent);
}
```

#### Step 2: Identify Bottlenecks
```c
void identify_bottlenecks() {
    #ifdef PICO_RTOS_ENABLE_EXECUTION_PROFILING
    // Profile critical functions
    pico_rtos_profiler_start();
    
    critical_function_1();
    pico_rtos_profiler_mark("function_1");
    
    critical_function_2();
    pico_rtos_profiler_mark("function_2");
    
    pico_rtos_profiler_stop();
    
    // Analyze results
    pico_rtos_profiler_stats_t stats;
    pico_rtos_profiler_get_stats(&stats);
    printf("Total execution: %lu us\n", stats.total_execution_time_us);
    #endif
}
```

### Memory Issues

#### Memory Leak Detection
```c
void detect_memory_leaks() {
    #ifdef PICO_RTOS_ENABLE_MEMORY_TRACKING
    // Take initial snapshot
    pico_rtos_memory_stats_t initial_stats;
    pico_rtos_get_memory_stats(&initial_stats);
    
    // Run suspected leaky code
    for (int i = 0; i < 100; i++) {
        potentially_leaky_function();
    }
    
    // Check for leaks
    pico_rtos_memory_stats_t final_stats;
    pico_rtos_get_memory_stats(&final_stats);
    
    if (final_stats.allocated_bytes > initial_stats.allocated_bytes) {
        printf("MEMORY LEAK DETECTED!\n");
        printf("Leaked: %lu bytes\n", 
               final_stats.allocated_bytes - initial_stats.allocated_bytes);
        
        // Show allocation details
        pico_rtos_show_memory_allocations();
    }
    #endif
}
```

### Multi-Core Issues *(v0.3.1)*

#### Core Utilization Analysis
```c
void analyze_core_utilization() {
    #ifdef PICO_RTOS_ENABLE_MULTI_CORE
    printf("=== Core Utilization Analysis ===\n");
    
    // Monitor over time
    for (int i = 0; i < 10; i++) {
        pico_rtos_system_stats_t stats;
        pico_rtos_get_system_stats(&stats);
        
        printf("Sample %d: Core0=%lu%%, Core1=%lu%%\n", 
               i, stats.cpu_usage_core0, stats.cpu_usage_core1);
        
        pico_rtos_task_delay(1000);
    }
    
    // Check load balancing effectiveness
    if (abs(stats.cpu_usage_core0 - stats.cpu_usage_core1) > 30) {
        printf("WARNING: Uneven core utilization!\n");
        printf("Consider adjusting task affinity or load balance threshold\n");
    }
    #endif
}
```

---

## 🛠️ Common Fixes

### Configuration Fixes

#### Fix 1: Reset to Known Good Configuration
```bash
# Load default configuration
cp config/defconfig .config
make configure
make build
```

#### Fix 2: Minimal Configuration for Debugging
```bash
# Use minimal profile to isolate issues
scripts/build.sh --profile minimal
```

### Code Fixes

#### Fix 1: Proper Error Handling
```c
// WRONG: Ignoring return values
pico_rtos_mutex_lock(&mutex, 1000);
do_critical_work();
pico_rtos_mutex_unlock(&mutex);

// CORRECT: Check return values
if (pico_rtos_mutex_lock(&mutex, 1000)) {
    do_critical_work();
    pico_rtos_mutex_unlock(&mutex);
} else {
    printf("Failed to acquire mutex\n");
    handle_error();
}
```

#### Fix 2: Proper Resource Cleanup
```c
// WRONG: Not cleaning up on error
void bad_function() {
    void *buffer = pico_rtos_malloc(1024);
    if (some_operation_fails()) {
        return;  // Memory leak!
    }
    pico_rtos_free(buffer);
}

// CORRECT: Always clean up
void good_function() {
    void *buffer = pico_rtos_malloc(1024);
    if (buffer == NULL) {
        return;
    }
    
    if (some_operation_fails()) {
        pico_rtos_free(buffer);
        return;
    }
    
    pico_rtos_free(buffer);
}
```

#### Fix 3: Stack Size Optimization
```c
// Check actual stack usage and adjust
void optimize_stack_sizes() {
    #ifdef PICO_RTOS_ENABLE_TASK_INSPECTION
    pico_rtos_task_info_t info;
    pico_rtos_debug_get_task_info(&my_task, &info);
    
    float usage = (float)info.stack_used / info.stack_size;
    printf("Stack usage: %.1f%%\n", usage * 100);
    
    if (usage < 0.5) {
        printf("Consider reducing stack size from %lu to %lu\n",
               info.stack_size, info.stack_used * 2);
    } else if (usage > 0.9) {
        printf("WARNING: Increase stack size from %lu to %lu\n",
               info.stack_size, info.stack_size * 2);
    }
    #endif
}
```

---

## 🚨 Emergency Procedures

### System Recovery

#### Hard Reset Recovery
```c
// Emergency system reset
void emergency_reset() {
    printf("EMERGENCY: Performing system reset\n");
    
    // Disable interrupts
    pico_rtos_enter_critical();
    
    // Reset hardware
    watchdog_enable(1, 1);  // 1ms timeout
    while (1) {
        // Wait for watchdog reset
    }
}
```

#### Safe Mode Boot
```c
// Boot in safe mode with minimal features
void safe_mode_boot() {
    printf("Booting in safe mode...\n");
    
    // Initialize with minimal configuration
    if (!pico_rtos_init()) {
        printf("CRITICAL: Safe mode init failed\n");
        emergency_reset();
    }
    
    // Create only essential tasks
    create_minimal_tasks();
    
    printf("Safe mode active - limited functionality\n");
    pico_rtos_start();
}
```

### Data Recovery

#### Save Critical Data
```c
void save_critical_data() {
    // Save to flash or external storage
    printf("Saving critical system state...\n");
    
    // Save system statistics
    pico_rtos_system_stats_t stats;
    if (pico_rtos_get_system_stats(&stats)) {
        save_to_flash(&stats, sizeof(stats));
    }
    
    // Save error history
    #ifdef PICO_RTOS_ENABLE_ERROR_HISTORY
    pico_rtos_error_entry_t errors[10];
    uint32_t count = 10;
    if (pico_rtos_get_error_history(errors, &count)) {
        save_to_flash(errors, count * sizeof(pico_rtos_error_entry_t));
    }
    #endif
}
```

---

## 📋 Troubleshooting Checklist

### Before Reporting Issues
- [ ] Check system information and version
- [ ] Verify configuration is correct
- [ ] Test with minimal configuration
- [ ] Check for known issues in documentation
- [ ] Collect debug information

### Debug Information to Collect
- [ ] Pico-RTOS version and configuration
- [ ] Hardware platform and toolchain version
- [ ] System statistics and task states
- [ ] Error messages and stack traces
- [ ] Reproduction steps
- [ ] Expected vs actual behavior

### Performance Issues
- [ ] Measure baseline performance
- [ ] Profile critical code paths
- [ ] Check memory usage patterns
- [ ] Analyze task priorities and scheduling
- [ ] Monitor multi-core utilization *(v0.3.1)*

### Memory Issues
- [ ] Enable memory tracking
- [ ] Check for memory leaks
- [ ] Validate stack sizes
- [ ] Monitor heap fragmentation
- [ ] Use memory pools where appropriate *(v0.3.1)*

### Multi-Core Issues *(v0.3.1)*
- [ ] Verify SMP initialization
- [ ] Check core utilization balance
- [ ] Validate task affinity settings
- [ ] Monitor inter-core communication
- [ ] Test single-core fallback

---

## 🆘 Getting Help

### Self-Help Resources
1. **Documentation**: Check [API Reference](api_reference.md) and [User Guide](user_guide.md)
2. **Examples**: Review working examples in `examples/` directory
3. **Tests**: Look at test cases in `tests/` directory
4. **Configuration**: Use `make menuconfig` to explore options

### Community Support
1. **GitHub Issues**: Report bugs and request features
2. **Discussions**: Ask questions in GitHub Discussions
3. **Wiki**: Check community-maintained wiki
4. **Examples**: Share and find community examples

### Professional Support
For commercial projects requiring professional support:
- Priority bug fixes and feature requests
- Custom development and integration
- Training and consulting services
- Extended validation and testing

---

**Troubleshooting Guide Version**: v0.3.1  
**Last Updated**: July 25, 2025  
**Covers**: All versions (v0.1.0 through v0.3.1)