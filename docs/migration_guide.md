# Pico-RTOS Migration Guide

This guide helps you migrate between different versions of Pico-RTOS and adopt new features.

**Current Version**: v0.3.0 "Advanced Synchronization & Multi-Core"

## 🔄 Migration Overview

### Supported Migration Paths
- **v0.2.1 → v0.3.0**: Seamless upgrade with 100% backward compatibility
- **v0.2.0 → v0.3.0**: Direct upgrade supported
- **v0.1.x → v0.3.0**: Upgrade via v0.2.1 recommended

### Migration Philosophy
- **Zero Breaking Changes**: All existing code continues to work
- **Additive Features**: New capabilities are optional
- **Incremental Adoption**: Adopt new features at your own pace
- **Configuration Migration**: Automated tools for settings

---

## 🚀 Migrating to v0.3.0

### Prerequisites
- Existing Pico-RTOS v0.2.1 or v0.2.0 project
- ARM GCC toolchain
- CMake 3.13 or later
- Python 3.6+ (for configuration tools)

### Step 1: Update Project Files

#### Update Git Repository
```bash
# Update to v0.3.0
git fetch origin
git checkout v0.3.0

# Update submodules if using
git submodule update --init --recursive
```

#### Update CMakeLists.txt
Your existing CMakeLists.txt will continue to work. Optionally update version references:

```cmake
# Optional: Update version reference
set(PICO_RTOS_VERSION "0.3.0")

# Your existing configuration continues to work
set(PICO_RTOS_MAX_TASKS 16)
set(PICO_RTOS_TICK_RATE_HZ 1000)
# ... rest of your configuration
```

### Step 2: Configuration Migration

#### Automatic Migration
Use the migration tool to automatically convert your configuration:

```bash
# Migrate configuration from v0.2.1
python3 scripts/menuconfig.py --migrate-from-v0.2.1

# Or use the interactive migration
make menuconfig
# Select "Migrate from v0.2.1" option
```

#### Manual Configuration Check
Verify your configuration is properly migrated:

```bash
# Show current configuration
make showconfig

# Generate configuration files
make configure
```

### Step 3: Build Verification

#### Test Existing Build
Your existing build should work without changes:

```bash
# Clean build to verify
make clean
make build

# Test with your existing examples
make flash-your_existing_example
```

#### Verify Backward Compatibility
```bash
# Run compatibility tests
make tests
./build/compatibility_test

# Check for any deprecation warnings
make build 2>&1 | grep -i deprecat
```

### Step 4: Optional Feature Adoption

You can now optionally adopt v0.3.0 features:

#### Enable New Synchronization Features
```cmake
# In your CMakeLists.txt or via menuconfig
set(PICO_RTOS_ENABLE_EVENT_GROUPS ON)
set(PICO_RTOS_ENABLE_STREAM_BUFFERS ON)
set(PICO_RTOS_ENABLE_MEMORY_POOLS ON)
```

#### Enable Multi-Core Support
```cmake
# Enable SMP scheduler
set(PICO_RTOS_ENABLE_MULTI_CORE ON)
set(PICO_RTOS_ENABLE_LOAD_BALANCING ON)
```

#### Enable Debugging Features
```cmake
# Enable debugging and profiling
set(PICO_RTOS_ENABLE_TASK_INSPECTION ON)
set(PICO_RTOS_ENABLE_EXECUTION_PROFILING ON)
set(PICO_RTOS_ENABLE_SYSTEM_TRACING ON)
```

---

## 🆕 New Features in v0.3.0

### Advanced Synchronization Primitives

#### Event Groups
Replace complex flag-based synchronization:

**Before (v0.2.1):**
```c
// Complex flag management
volatile uint32_t event_flags = 0;
pico_rtos_mutex_t flag_mutex;

void set_event(uint32_t event) {
    pico_rtos_mutex_lock(&flag_mutex, PICO_RTOS_WAIT_FOREVER);
    event_flags |= event;
    pico_rtos_mutex_unlock(&flag_mutex);
}

void wait_for_events() {
    while (1) {
        pico_rtos_mutex_lock(&flag_mutex, PICO_RTOS_WAIT_FOREVER);
        if (event_flags & (EVENT_A | EVENT_B)) {
            event_flags &= ~(EVENT_A | EVENT_B);
            pico_rtos_mutex_unlock(&flag_mutex);
            break;
        }
        pico_rtos_mutex_unlock(&flag_mutex);
        pico_rtos_task_delay(10);
    }
}
```

**After (v0.3.0):**
```c
// Clean event group implementation
pico_rtos_event_group_t events;

void init_events() {
    pico_rtos_event_group_init(&events);
}

void set_event(uint32_t event) {
    pico_rtos_event_group_set_bits(&events, event);
}

void wait_for_events() {
    uint32_t result = pico_rtos_event_group_wait_bits(
        &events,
        EVENT_A | EVENT_B,  // Wait for any of these
        false,              // Wait for ANY (not all)
        true,               // Clear after wait
        PICO_RTOS_WAIT_FOREVER
    );
}
```

#### Stream Buffers
Replace fixed-size queues for variable data:

**Before (v0.2.1):**
```c
// Fixed-size message queue
typedef struct {
    uint8_t data[256];
    uint32_t length;
} message_t;

pico_rtos_queue_t message_queue;
message_t queue_buffer[10];

void init_messaging() {
    pico_rtos_queue_init(&message_queue, queue_buffer, 
                        sizeof(message_t), 10);
}
```

**After (v0.3.0):**
```c
// Variable-length stream buffer
pico_rtos_stream_buffer_t stream;
uint8_t stream_buffer[2048];

void init_messaging() {
    pico_rtos_stream_buffer_init(&stream, stream_buffer, 
                                sizeof(stream_buffer));
}

void send_variable_message(const void *data, uint32_t size) {
    pico_rtos_stream_buffer_send(&stream, data, size, 1000);
}
```

### Multi-Core Programming

#### Basic Multi-Core Setup
```c
// Enable multi-core in your main()
int main() {
    pico_rtos_init();
    pico_rtos_smp_init();  // NEW: Enable SMP
    
    // Create tasks as before - they'll automatically load balance
    pico_rtos_task_create(&task1, "Task1", task1_func, NULL, 1024, 10);
    pico_rtos_task_create(&task2, "Task2", task2_func, NULL, 1024, 10);
    
    pico_rtos_start();
    return 0;
}
```

#### Core Affinity (Optional)
```c
// Bind specific tasks to specific cores
pico_rtos_task_t critical_task;
pico_rtos_task_create(&critical_task, "Critical", critical_func, NULL, 1024, 20);

// Run only on core 0
pico_rtos_task_set_core_affinity(&critical_task, 0x01);
```

### Enhanced Debugging

#### Task Inspection
```c
// Get detailed task information
pico_rtos_task_info_t task_info;
if (pico_rtos_debug_get_task_info(NULL, &task_info)) {
    printf("Task: %s, Stack used: %lu/%lu bytes\n",
           task_info.name, task_info.stack_used, task_info.stack_size);
}
```

#### Execution Profiling
```c
// Profile your application
pico_rtos_profiler_start();

// Your application code here
your_critical_function();

pico_rtos_profiler_stop();

// Get profiling results
pico_rtos_profiler_stats_t stats;
pico_rtos_profiler_get_stats(&stats);
printf("Function took %lu microseconds\n", stats.execution_time_us);
```

---

## ⚙️ Configuration Changes

### New Configuration Options

#### v0.3.0 Feature Toggles
```cmake
# Advanced Synchronization
PICO_RTOS_ENABLE_EVENT_GROUPS=ON
PICO_RTOS_ENABLE_STREAM_BUFFERS=ON

# Memory Management
PICO_RTOS_ENABLE_MEMORY_POOLS=ON
PICO_RTOS_ENABLE_MPU_SUPPORT=OFF  # Requires hardware MPU

# Multi-Core Support
PICO_RTOS_ENABLE_MULTI_CORE=ON
PICO_RTOS_ENABLE_LOAD_BALANCING=ON
PICO_RTOS_ENABLE_CORE_AFFINITY=ON

# Debugging & Profiling
PICO_RTOS_ENABLE_TASK_INSPECTION=ON
PICO_RTOS_ENABLE_EXECUTION_PROFILING=ON
PICO_RTOS_ENABLE_SYSTEM_TRACING=ON

# Quality Assurance
PICO_RTOS_ENABLE_DEADLOCK_DETECTION=OFF  # Development only
PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING=ON
PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION=ON
```

#### Resource Limits
```cmake
# Adjust limits for new features
PICO_RTOS_MAX_EVENT_GROUPS=8
PICO_RTOS_MAX_STREAM_BUFFERS=4
PICO_RTOS_MAX_MEMORY_POOLS=4
PICO_RTOS_TRACE_BUFFER_SIZE=256
```

### Configuration Profiles

#### Use Pre-Configured Profiles
```bash
# Minimal footprint build
scripts/build.sh --profile minimal

# Standard features (recommended)
scripts/build.sh --profile default

# All features enabled
scripts/build.sh --profile full
```

---

## 🔧 Build System Updates

### New Build Targets

#### Enhanced Build Options
```bash
# Build with specific profile
make build PROFILE=full

# Build with multi-core support
make build ENABLE_MULTI_CORE=ON

# Build with all debugging features
make build ENABLE_DEBUG_FEATURES=ON
```

#### New Test Targets
```bash
# Run v0.3.0 specific tests
make test-event-groups
make test-stream-buffers
make test-multi-core

# Run comprehensive test suite
scripts/test.sh --all
```

### Updated Scripts

#### New Configuration System
```bash
# Interactive configuration (enhanced)
make menuconfig

# GUI configuration
make guiconfig

# Show current configuration
make showconfig

# Validate configuration
scripts/validate.sh --config-only
```

---

## 🐛 Troubleshooting Migration

### Common Issues

#### Build Errors After Migration

**Issue**: Compilation errors after updating
```
error: 'PICO_RTOS_ENABLE_EVENT_GROUPS' undeclared
```

**Solution**: Regenerate configuration files
```bash
make clean
make configure
make build
```

#### Configuration Not Taking Effect

**Issue**: New features not available despite being enabled

**Solution**: Verify configuration generation
```bash
# Check generated config
cat cmake_config.cmake | grep EVENT_GROUPS

# Regenerate if needed
python3 scripts/menuconfig.py --generate
```

#### Multi-Core Issues

**Issue**: Tasks not distributing across cores

**Solution**: Verify SMP initialization
```c
// Ensure SMP is initialized
if (!pico_rtos_smp_init()) {
    printf("SMP initialization failed\n");
}

// Check if running on dual-core hardware
if (get_core_num() == 0) {
    printf("Running on core 0\n");
}
```

### Performance Issues

#### Increased Memory Usage

**Issue**: Higher memory consumption after migration

**Solution**: Optimize configuration for your needs
```bash
# Use minimal profile
scripts/build.sh --profile minimal

# Or disable unused features
make menuconfig
# Disable features you don't need
```

#### Slower Performance

**Issue**: Performance regression after enabling new features

**Solution**: Profile and optimize
```bash
# Run performance tests
scripts/test.sh --performance

# Profile your application
# Enable profiling in your code and analyze results
```

### Getting Help

#### Debug Migration Issues
```bash
# Run migration validation
scripts/validate.sh --migration

# Check compatibility
make test-compatibility

# Get detailed build information
make debug-config
```

#### Support Resources
- Check [Troubleshooting Guide](troubleshooting.md)
- Review [API Reference](api_reference.md)
- Run validation scripts
- Open GitHub issue with migration details

---

## 📈 Performance Considerations

### Memory Usage

#### v0.3.0 Memory Impact
- **Event Groups**: ~32 bytes per group
- **Stream Buffers**: Buffer size + ~64 bytes overhead
- **Memory Pools**: Pool size + ~48 bytes overhead
- **Multi-Core**: ~256 bytes additional overhead
- **Debugging Features**: ~512 bytes when enabled

#### Optimization Tips
```cmake
# Minimize memory usage
set(PICO_RTOS_MAX_EVENT_GROUPS 4)      # Reduce if not needed
set(PICO_RTOS_MAX_STREAM_BUFFERS 2)    # Reduce if not needed
set(PICO_RTOS_TRACE_BUFFER_SIZE 64)    # Smaller trace buffer

# Disable unused features
set(PICO_RTOS_ENABLE_SYSTEM_TRACING OFF)
set(PICO_RTOS_ENABLE_EXECUTION_PROFILING OFF)
```

### CPU Performance

#### Multi-Core Benefits
- **Task Parallelism**: True parallel execution on RP2040
- **Load Balancing**: Automatic workload distribution
- **Reduced Contention**: Less blocking on single-core bottlenecks

#### Performance Monitoring
```c
// Monitor system performance
pico_rtos_system_stats_t stats;
pico_rtos_get_system_stats(&stats);

printf("CPU Usage: Core 0: %lu%%, Core 1: %lu%%\n",
       stats.cpu_usage_core0, stats.cpu_usage_core1);
```

---

## ✅ Migration Checklist

### Pre-Migration
- [ ] Backup your current working project
- [ ] Document your current configuration
- [ ] Test your current build and functionality
- [ ] Review v0.3.0 changelog and new features

### Migration Process
- [ ] Update to v0.3.0 codebase
- [ ] Run configuration migration tool
- [ ] Verify build compiles successfully
- [ ] Test existing functionality works
- [ ] Run compatibility tests

### Post-Migration
- [ ] Evaluate new features for your project
- [ ] Update documentation and comments
- [ ] Consider adopting new synchronization primitives
- [ ] Evaluate multi-core benefits for your application
- [ ] Update your testing procedures

### Optional Enhancements
- [ ] Adopt event groups for complex synchronization
- [ ] Use stream buffers for variable-length messaging
- [ ] Enable multi-core support for performance
- [ ] Add debugging and profiling capabilities
- [ ] Implement health monitoring

---

## 🎯 Best Practices

### Gradual Feature Adoption
1. **Start Conservative**: Begin with minimal configuration
2. **Test Thoroughly**: Validate each new feature individually
3. **Monitor Performance**: Check memory and CPU impact
4. **Document Changes**: Keep track of configuration changes

### Multi-Core Development
1. **Design for Parallelism**: Consider task independence
2. **Minimize Shared State**: Reduce inter-core synchronization
3. **Use Core Affinity Wisely**: Pin critical tasks when needed
4. **Monitor Load Balance**: Ensure even core utilization

### Debugging Strategy
1. **Enable Gradually**: Start with basic inspection
2. **Profile Selectively**: Focus on critical code paths
3. **Use Tracing Sparingly**: Enable only when debugging
4. **Monitor System Health**: Keep health monitoring enabled

---

**Migration Guide Version**: v0.3.0  
**Last Updated**: July 25, 2025  
**Supported Migrations**: v0.2.1→v0.3.0, v0.2.0→v0.3.0