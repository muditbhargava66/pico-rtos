# Migration Guide: Upgrading from Pico-RTOS v0.2.1 to v0.3.1

This guide provides comprehensive instructions for upgrading existing Pico-RTOS v0.2.1 applications to v0.3.1, including configuration changes, new features, and troubleshooting common migration issues.

## Overview

Pico-RTOS v0.3.1 is designed to be fully backward compatible with v0.2.1 applications. All existing APIs remain unchanged, and existing applications should compile and run without modification. However, to take advantage of the new features, some configuration and code updates may be desired.

### What's New in v0.3.1

- **Advanced Synchronization Primitives**: Event groups and stream buffers
- **Enhanced Memory Management**: Memory pools and optional MPU support
- **Comprehensive Debugging Tools**: Task inspection, profiling, and tracing
- **Multi-Core Support**: SMP scheduling and inter-core communication
- **Advanced System Extensions**: High-resolution timers and thread-safe I/O
- **Production-Grade Quality Assurance**: Deadlock detection and health monitoring

## Migration Process

### Step 1: Backup Your Project

Before starting the migration, create a complete backup of your existing project:

```bash
# Create a backup directory
mkdir pico-rtos-v021-backup
cp -r your-project/* pico-rtos-v021-backup/
```

### Step 2: Update Pico-RTOS Version

#### Option A: Git Submodule Update
If you're using Pico-RTOS as a git submodule:

```bash
cd your-project/pico-rtos
git fetch origin
git checkout v0.3.1
cd ..
git add pico-rtos
git commit -m "Update Pico-RTOS to v0.3.1"
```

#### Option B: Manual Download
1. Download Pico-RTOS v0.3.1 from the releases page
2. Replace your existing `pico-rtos` directory
3. Update any local modifications

### Step 3: Update Build Configuration

#### CMakeLists.txt Changes

Your existing CMakeLists.txt should continue to work without changes. However, you may want to add new optional components:

```cmake
# Existing configuration (no changes required)
target_link_libraries(your_project
    pico_stdlib
    pico_rtos
)

# Optional: Add new v0.3.1 components
target_link_libraries(your_project
    pico_stdlib
    pico_rtos
    # New optional libraries (if using specific features)
    pico_rtos_event_groups      # For event group functionality
    pico_rtos_stream_buffers    # For stream buffer functionality
    pico_rtos_memory_pools      # For memory pool functionality
    pico_rtos_multicore        # For multi-core functionality
    pico_rtos_profiling        # For debugging and profiling
)
```

#### Kconfig/Menuconfig Updates

Run the configuration tool to see new options:

```bash
# Update configuration with new v0.3.1 options
./menuconfig.sh
```

New configuration sections include:
- **Advanced Synchronization**: Event groups and stream buffers
- **Memory Management**: Memory pools and MPU support
- **Multi-Core Support**: SMP scheduling and load balancing
- **Debugging and Profiling**: Task inspection and performance monitoring
- **System Extensions**: High-resolution timers and I/O abstraction

### Step 4: Verify Compatibility

#### Compile Test
Compile your existing application without any changes:

```bash
mkdir build
cd build
cmake ..
make
```

Your application should compile successfully. If you encounter errors, see the [Troubleshooting](#troubleshooting) section.

#### Runtime Test
Flash and test your application to ensure it behaves identically to v0.2.1:

```bash
# Flash to device
picotool load your_project.uf2

# Monitor output for any behavioral changes
# Your application should run exactly as before
```

## Configuration Migration

### Automatic Migration Tool

Pico-RTOS v0.3.1 includes an automatic configuration migration tool:

```bash
# Migrate v0.2.1 configuration to v0.3.1
python3 scripts/migrate_config.py --input config/.config --output config/v030_config.cmake

# Review the migrated configuration
cat config/v030_config.cmake
```

### Manual Configuration Updates

#### Core Configuration (No Changes Required)

All existing core configuration options remain unchanged:

```cmake
# These settings remain the same
set(PICO_RTOS_MAX_TASKS 32)
set(PICO_RTOS_TICK_RATE_HZ 1000)
set(PICO_RTOS_STACK_OVERFLOW_CHECK ON)
set(PICO_RTOS_ENABLE_LOGGING ON)
```

#### New Optional Features

Add new features as needed:

```cmake
# Event Groups (optional)
set(PICO_RTOS_ENABLE_EVENT_GROUPS ON)
set(PICO_RTOS_MAX_EVENT_GROUPS 16)

# Stream Buffers (optional)
set(PICO_RTOS_ENABLE_STREAM_BUFFERS ON)
set(PICO_RTOS_MAX_STREAM_BUFFERS 8)

# Memory Pools (optional)
set(PICO_RTOS_ENABLE_MEMORY_POOLS ON)
set(PICO_RTOS_MAX_MEMORY_POOLS 4)

# Multi-Core Support (optional)
set(PICO_RTOS_ENABLE_MULTICORE ON)
set(PICO_RTOS_ENABLE_LOAD_BALANCING ON)

# Debugging and Profiling (optional)
set(PICO_RTOS_ENABLE_PROFILING ON)
set(PICO_RTOS_ENABLE_TRACING ON)
set(PICO_RTOS_PROFILER_MAX_ENTRIES 100)
set(PICO_RTOS_TRACE_BUFFER_SIZE 1000)

# High-Resolution Timers (optional)
set(PICO_RTOS_ENABLE_HIRES_TIMERS ON)
set(PICO_RTOS_MAX_HIRES_TIMERS 16)
```

## Taking Advantage of New Features

### Adding Event Groups

Replace complex synchronization patterns with event groups:

#### Before (v0.2.1):
```c
// Complex synchronization using multiple semaphores
pico_rtos_semaphore_t sensor_ready_sem;
pico_rtos_semaphore_t network_ready_sem;
pico_rtos_semaphore_t storage_ready_sem;

// Wait for all subsystems (complex logic required)
pico_rtos_semaphore_take(&sensor_ready_sem, PICO_RTOS_WAIT_FOREVER);
pico_rtos_semaphore_take(&network_ready_sem, PICO_RTOS_WAIT_FOREVER);
pico_rtos_semaphore_take(&storage_ready_sem, PICO_RTOS_WAIT_FOREVER);
```

#### After (v0.3.1):
```c
// Simple event group coordination
pico_rtos_event_group_t system_events;

#define EVENT_SENSOR_READY   (1 << 0)
#define EVENT_NETWORK_READY  (1 << 1)
#define EVENT_STORAGE_READY  (1 << 2)
#define EVENTS_ALL_READY     (EVENT_SENSOR_READY | EVENT_NETWORK_READY | EVENT_STORAGE_READY)

// Wait for all subsystems (simple one-liner)
uint32_t events = pico_rtos_event_group_wait_bits(&system_events, 
                                                  EVENTS_ALL_READY, 
                                                  true,    // wait for all
                                                  false,   // don't clear
                                                  10000);  // 10s timeout
```

### Adding Stream Buffers

Replace queue-based variable-length messaging with stream buffers:

#### Before (v0.2.1):
```c
// Complex variable-length messaging using queues
typedef struct {
    uint32_t length;
    uint8_t data[MAX_MESSAGE_SIZE];
} variable_message_t;

pico_rtos_queue_t message_queue;
variable_message_t queue_buffer[QUEUE_SIZE];
```

#### After (v0.3.1):
```c
// Simple stream buffer for variable-length messages
pico_rtos_stream_buffer_t message_stream;
uint8_t stream_buffer[STREAM_BUFFER_SIZE];

// Send variable-length data directly
uint32_t bytes_sent = pico_rtos_stream_buffer_send(&message_stream, 
                                                   data, data_length, 1000);

// Receive variable-length data directly
uint32_t bytes_received = pico_rtos_stream_buffer_receive(&message_stream,
                                                         buffer, buffer_size, 1000);
```

### Adding Memory Pools

Replace malloc/free with deterministic memory pools:

#### Before (v0.2.1):
```c
// Non-deterministic memory allocation
void *buffer = pico_rtos_malloc(BUFFER_SIZE);
if (buffer) {
    // Use buffer
    pico_rtos_free(buffer, BUFFER_SIZE);
}
```

#### After (v0.3.1):
```c
// Deterministic memory pool allocation
static uint8_t pool_memory[BUFFER_SIZE * POOL_COUNT];
pico_rtos_memory_pool_t buffer_pool;

// Initialize pool once
pico_rtos_memory_pool_init(&buffer_pool, pool_memory, BUFFER_SIZE, POOL_COUNT);

// O(1) allocation and deallocation
void *buffer = pico_rtos_memory_pool_alloc(&buffer_pool, 1000);
if (buffer) {
    // Use buffer
    pico_rtos_memory_pool_free(&buffer_pool, buffer);
}
```

### Adding Multi-Core Support

Distribute tasks across both RP2040 cores:

#### Before (v0.2.1):
```c
// Single-core task creation
pico_rtos_task_create(&task, "MyTask", task_function, NULL, 1024, 5);
```

#### After (v0.3.1):
```c
// Multi-core task creation with affinity
pico_rtos_task_create(&task, "MyTask", task_function, NULL, 1024, 5);

// Set core affinity
pico_rtos_task_set_core_affinity(&task, PICO_RTOS_CORE_AFFINITY_CORE1);

// Or allow load balancing
pico_rtos_task_set_core_affinity(&task, PICO_RTOS_CORE_AFFINITY_ANY);

// Enable load balancing
pico_rtos_smp_enable_load_balancing(true);
```

### Adding Debugging and Profiling

Add comprehensive debugging capabilities:

```c
// Initialize profiling (in main)
pico_rtos_profiler_init(100);
pico_rtos_profiler_start();

// Profile critical functions
void critical_function(void) {
    PICO_RTOS_PROFILE_FUNCTION_START(FUNC_ID_CRITICAL);
    
    // Function implementation
    
    PICO_RTOS_PROFILE_FUNCTION_END(FUNC_ID_CRITICAL);
}

// Get task information for debugging
pico_rtos_task_info_t task_info;
if (pico_rtos_get_task_info(&my_task, &task_info)) {
    printf("Task: %s, CPU time: %llu μs, Stack usage: %lu bytes\n",
           task_info.name, task_info.cpu_time_us, task_info.stack_usage);
}
```

## Performance Tuning for v0.3.1

### Memory Optimization

Configure features based on your needs to minimize memory usage:

```cmake
# Minimal configuration for memory-constrained applications
set(PICO_RTOS_ENABLE_EVENT_GROUPS OFF)
set(PICO_RTOS_ENABLE_STREAM_BUFFERS OFF)
set(PICO_RTOS_ENABLE_MEMORY_POOLS OFF)
set(PICO_RTOS_ENABLE_MULTICORE OFF)
set(PICO_RTOS_ENABLE_PROFILING OFF)
set(PICO_RTOS_ENABLE_TRACING OFF)

# Maximum performance configuration
set(PICO_RTOS_ENABLE_EVENT_GROUPS ON)
set(PICO_RTOS_ENABLE_STREAM_BUFFERS ON)
set(PICO_RTOS_ENABLE_MEMORY_POOLS ON)
set(PICO_RTOS_ENABLE_MULTICORE ON)
set(PICO_RTOS_ENABLE_LOAD_BALANCING ON)
```

### Multi-Core Performance

Optimize task distribution for dual-core performance:

```c
// Bind time-critical tasks to specific cores
pico_rtos_task_set_core_affinity(&realtime_task, PICO_RTOS_CORE_AFFINITY_CORE0);

// Allow background tasks to be load balanced
pico_rtos_task_set_core_affinity(&background_task, PICO_RTOS_CORE_AFFINITY_ANY);

// Configure load balancing threshold
pico_rtos_smp_set_load_balance_threshold(15);  // 15% imbalance threshold
```

### Debugging Performance Impact

Monitor the performance impact of debugging features:

| Feature | Memory Overhead | Runtime Overhead | Recommended Use |
|---------|----------------|------------------|-----------------|
| Task Inspection | ~100 bytes | ~5-10 μs per call | Development only |
| Function Profiling | ~32 bytes per function | ~1-2 μs per entry/exit | Development/testing |
| Event Tracing | ~16 bytes per event | ~0.5-1 μs per event | Development/debugging |
| Memory Pool Stats | ~32 bytes per pool | ~1 μs per operation | Production OK |

## Troubleshooting

### Common Migration Issues

#### Issue 1: Compilation Errors

**Problem**: Undefined references to new v0.3.1 functions.

**Solution**: 
```cmake
# Ensure you're linking against the correct version
find_package(PicoRTOS 0.3.1 REQUIRED)
target_link_libraries(your_project pico_rtos)
```

#### Issue 2: Increased Memory Usage

**Problem**: Application uses more memory after upgrade.

**Solution**: 
```cmake
# Disable unused features to reduce memory footprint
set(PICO_RTOS_ENABLE_PROFILING OFF)
set(PICO_RTOS_ENABLE_TRACING OFF)
set(PICO_RTOS_PROFILER_MAX_ENTRIES 0)
set(PICO_RTOS_TRACE_BUFFER_SIZE 0)
```

#### Issue 3: Multi-Core Initialization Failures

**Problem**: Multi-core features fail to initialize.

**Solution**:
```c
// Initialize multi-core support explicitly
if (!pico_rtos_ipc_init()) {
    printf("ERROR: Failed to initialize IPC system\n");
    // Handle error appropriately
}
```

#### Issue 4: Performance Regression

**Problem**: Application runs slower after upgrade.

**Solution**:
```c
// Disable debugging features in production builds
#ifdef NDEBUG
    // Production build - disable debugging
    pico_rtos_profiler_stop();
    pico_rtos_trace_stop();
#else
    // Debug build - enable debugging
    pico_rtos_profiler_start();
    pico_rtos_trace_start();
#endif
```

### Configuration Validation

Use the built-in configuration validator:

```bash
# Validate your configuration
python3 scripts/validate_config.py config/v030_config.cmake

# Check for common configuration issues
python3 scripts/check_config_compatibility.py
```

### Migration Testing Checklist

- [ ] Application compiles without errors
- [ ] Application runs with identical behavior to v0.2.1
- [ ] Memory usage is within acceptable limits
- [ ] Performance meets requirements
- [ ] All existing functionality works correctly
- [ ] New features (if used) work as expected
- [ ] Multi-core functionality (if enabled) works correctly
- [ ] Debugging features (if enabled) provide useful information

## Best Practices for v0.3.1

### 1. Gradual Feature Adoption

Don't enable all new features at once. Add them incrementally:

1. **Phase 1**: Migrate to v0.3.1 with existing functionality
2. **Phase 2**: Add event groups or stream buffers where beneficial
3. **Phase 3**: Enable multi-core support if needed
4. **Phase 4**: Add debugging and profiling capabilities

### 2. Configuration Management

Use version-controlled configuration files:

```bash
# Keep separate configurations for different builds
config/
├── debug_v030.cmake      # Full debugging enabled
├── release_v030.cmake    # Optimized for production
├── minimal_v030.cmake    # Minimal footprint
└── multicore_v030.cmake  # Multi-core enabled
```

### 3. Testing Strategy

Implement comprehensive testing for the migration:

```c
// Add migration validation tests
void test_v021_compatibility(void) {
    // Test that existing functionality works identically
    assert(existing_function_behavior_unchanged());
}

void test_v030_new_features(void) {
    // Test new features work correctly
    assert(event_groups_work_correctly());
    assert(stream_buffers_work_correctly());
}
```

### 4. Documentation Updates

Update your project documentation:

- Update API usage examples
- Document new configuration options
- Update performance characteristics
- Add troubleshooting guides

### 5. Monitoring and Metrics

Use v0.3.1's monitoring capabilities to validate the migration:

```c
// Monitor system health after migration
void monitor_migration_health(void) {
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    
    // Log key metrics
    printf("Post-migration metrics:\n");
    printf("  Memory usage: %lu bytes\n", stats.current_memory);
    printf("  Task count: %lu\n", stats.total_tasks);
    printf("  Context switches: %lu\n", stats.context_switches);
}
```

## Support and Resources

### Documentation

- [API Reference v0.3.1](api_reference_v0.3.1.md)
- [Multi-Core Programming Guide](multicore_programming_guide.md)
- [Performance Tuning Guide](performance_guide_v0.3.1.md)
- [Troubleshooting Guide](troubleshooting.md)

### Example Applications

Study the provided examples for migration patterns:

- `examples/event_group_coordination/` - Event group usage patterns
- `examples/stream_buffer_data_streaming/` - Stream buffer implementation
- `examples/multicore_load_balancing/` - Multi-core task distribution
- `examples/debugging_profiling_analysis/` - Debugging and profiling setup

### Migration Tools

Use the provided migration utilities:

```bash
# Configuration migration
scripts/migrate_config.py

# Compatibility checking
scripts/check_compatibility.py

# Performance analysis
scripts/analyze_performance.py
```

### Getting Help

If you encounter issues during migration:

1. Check the [Troubleshooting Guide](troubleshooting.md)
2. Review the [FAQ](faq.md)
3. Search existing issues on GitHub
4. Create a new issue with:
   - Your v0.2.1 configuration
   - Migration steps attempted
   - Error messages or unexpected behavior
   - Hardware platform details

## Conclusion

Migrating from Pico-RTOS v0.2.1 to v0.3.1 should be straightforward due to the maintained backward compatibility. The new features provide significant enhancements for complex applications while maintaining the real-time performance characteristics that make Pico-RTOS suitable for demanding embedded applications.

Take advantage of the new capabilities gradually, and use the comprehensive debugging and profiling tools to ensure your application continues to meet its real-time requirements while benefiting from the enhanced functionality.