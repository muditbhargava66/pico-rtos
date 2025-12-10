# Pico-RTOS Changelog

All notable changes to Pico-RTOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.1] - 2025-12-10 - **BUG FIX RELEASE**

### Enhanced
- **Documentation**: Updated all documentation to reflect v0.3.1, including new specific guides for API, Migration, Performance, and Troubleshooting.

### Fixed
- **Tests**: Fixed integer overflow in `stream_buffer_performance_test.c` throughput calculation.
- Fixed compilation and linking errors in `release/v0.3.1`:
  - **Version Macro Redefinition**: Fixed `PICO_RTOS_VERSION_...` macro redefinitions in `include/pico_rtos.h`.
  - **Stream Buffer Type Mismatch**: Merged `pico_rtos_stream_buffer_error_t` into `pico_rtos_error_t` to resolve type incompatibilities.
  - **Linker Errors**: Added missing `pico_multicore` and `pico_sync` dependencies to `pico_rtos` library target.
  - **Deprecation Warnings**: Fixed missing prototype for `pico_rtos_check_configuration_warnings` in `include/pico_rtos/deprecation.h`.
  - **Test Suite**: Fixed implicit function declaration errors in `tests/deprecation_test.c`.
  - **Build Warnings**: Supressed internal migration warnings to clean up build output.

- Fixed compilation error in `smp.c` with implicit declaration of `multicore_launch_core1` ([#3](https://github.com/muditbhargava66/pico-rtos/issues/3))
  - Replaced non-standard `PICO_PLATFORM` macro with standard Pico SDK `PICO_ON_DEVICE` macro
  - Added proper guards around multicore hardware functions
  - Added fallback behavior for non-device builds (testing environments)

- Fixed `make menuconfig` failing with unrecognized arguments error ([#4](https://github.com/muditbhargava66/pico-rtos/issues/4))
  - Added missing command-line argument support (`--config-file`, `--cmake-file`, `--header-file`)
  - Fixed configuration file generation to properly iterate over Kconfig symbols
  - Fixed defconfig file format to use proper `CONFIG_` prefix for kconfiglib compatibility
  - Added `--show-config` and `--load-defaults` argument aliases for compatibility

## [0.3.0] - 2025-07-26 - **ADVANCED SYNCHRONIZATION & MULTI-CORE** 🚀

### 🎯 Major Features

#### Advanced Synchronization Primitives
- **Event Groups**: 32-bit event coordination with any/all semantics
  - `pico_rtos_event_group_t` with O(1) bit manipulation
  - Support for "wait any" and "wait all" event patterns
  - Atomic event setting/clearing with proper synchronization
  - Comprehensive statistics and monitoring APIs
  - Zero-copy event notification for high-performance scenarios

- **Stream Buffers**: Efficient variable-length message passing
  - Circular buffer management with head/tail pointers
  - Zero-copy optimization for messages larger than threshold (configurable)
  - Thread-safe send/receive operations with timeout support
  - Direct buffer access APIs for high-performance data streaming
  - Variable-length message support with automatic size handling

#### Enhanced Memory Management
- **Memory Pools**: O(1) deterministic memory allocation
  - Fixed-size block allocation with predictable performance
  - Free list management with proper linkage and validation
  - Block corruption detection and comprehensive error handling
  - Runtime pool inspection and debugging capabilities
  - Allocation tracking with peak usage monitoring and fragmentation metrics

- **Memory Protection Unit (MPU) Support**: Hardware memory protection
  - MPU region configuration for supported ARM Cortex-M variants
  - Memory protection violation handlers with detailed diagnostics
  - Stack overflow detection using hardware MPU capabilities
  - Task isolation and memory permission management
  - Conditional compilation support for MPU-enabled/disabled builds

#### Comprehensive Debugging and Profiling Tools
- **Runtime Task Inspection**: Non-intrusive task monitoring
  - Task state collection without affecting real-time behavior
  - Stack usage monitoring with high-water mark tracking
  - Task execution statistics and performance metrics
  - Real-time task state visualization and debugging support

- **Execution Time Profiling**: High-resolution performance analysis
  - Hardware timer-based profiling using RP2040 capabilities
  - Function entry/exit hooks with minimal overhead
  - Statistical analysis (min/max/average execution times)
  - Performance regression detection and benchmarking

- **System Event Tracing**: Configurable event logging
  - Circular trace buffer with configurable overflow behavior
  - Trace event generation hooks throughout the RTOS
  - Real-time system behavior analysis and debugging
  - Trace buffer integrity validation and performance monitoring

- **Enhanced Assertion Handling**: Advanced debugging support
  - Assertion macros with detailed context capture
  - Configurable assertion handlers (halt/continue/callback)
  - Assertion statistics and failure tracking
  - Enhanced debugging information for development and testing

#### Multi-Core Support (SMP)
- **Symmetric Multiprocessing Scheduler**: RP2040 dual-core support
  - Per-core state management and independent task lists
  - Core affinity support for deterministic task placement
  - Task migration between cores with load balancing
  - Inter-core synchronization using RP2040 hardware primitives

- **Load Balancing**: Automatic workload distribution
  - Task assignment algorithms for optimal core utilization
  - Configurable load balancing thresholds and policies
  - CPU usage tracking per core and per task
  - Dynamic task migration based on system load

- **Inter-Core Communication**: High-performance IPC
  - Inter-core communication channels using hardware FIFOs
  - Thread-safe operation of all synchronization primitives across cores
  - Core failure detection and graceful degradation to single-core
  - Watchdog-based core health monitoring and recovery

#### Advanced System Extensions
- **Thread-Safe I/O Abstraction**: Unified peripheral access
  - Device abstraction APIs with automatic serialization
  - Device-specific handle management with proper cleanup
  - Concurrent I/O access protection and resource management

- **High-Resolution Software Timers**: Microsecond precision
  - Timer management using RP2040 hardware timer capabilities
  - Precise timing with drift compensation and calibration
  - Microsecond-resolution APIs for time-critical applications

- **Universal Timeout Support**: Consistent timeout handling
  - Timeout parameter handling for all blocking operations
  - Timeout accuracy and proper cleanup on expiration
  - Timeout statistics and monitoring capabilities

- **Multi-Level Logging System**: Enhanced debugging
  - Extended logging system with additional levels and filtering
  - Log message formatting and output redirection
  - Runtime log level configuration and subsystem filtering
  - Performance-optimized logging with minimal overhead

#### Production-Grade Quality Assurance
- **Deadlock Detection System**: Runtime deadlock prevention
  - Resource dependency tracking and cycle detection
  - Configurable deadlock detection with detailed reporting
  - Automatic deadlock resolution and recovery mechanisms

- **System Health Monitoring**: Comprehensive system metrics
  - CPU usage tracking per core and per task
  - Memory usage monitoring with leak detection
  - System health metrics collection and reporting
  - Real-time performance monitoring and alerting

- **Hardware Watchdog Integration**: System reliability
  - Watchdog timer configuration and management
  - Automatic watchdog feeding during normal operation
  - Watchdog timeout handlers with system recovery
  - Hardware-based system reliability and fault tolerance

- **Configurable Alert System**: Proactive monitoring
  - Threshold-based alerting for system metrics
  - Callback-based notification system for critical events
  - Alert escalation and acknowledgment mechanisms
  - Customizable alert policies and response actions

#### Backward Compatibility and Migration Support
- **100% API Compatibility**: Seamless upgrade from v0.2.1
  - All existing function signatures remain unchanged
  - Existing behavior preserved exactly for compatibility
  - Automatic configuration migration from v0.2.1 settings
  - Comprehensive compatibility testing with v0.2.1 applications

- **Migration Tools and Documentation**: Smooth transition
  - Automatic migration from v0.2.1 menuconfig settings
  - Validation for deprecated configuration options
  - Step-by-step migration documentation and guides
  - Troubleshooting guide for common migration issues

- **Feature Deprecation Warnings**: Clear upgrade path
  - Compile-time warnings for deprecated APIs and configurations
  - Clear alternatives and migration paths in warning messages
  - Deprecation timeline and removal schedule
  - Comprehensive testing for warning generation

### 🛠️ Enhanced Build System and Toolchain Support

#### Multi-Toolchain Support
- **Extended Toolchain Support**: GCC, Clang, and ARM toolchains
  - Toolchain-specific optimization flags and configurations
  - Cross-platform build support (Linux, macOS, Windows)
  - Automatic toolchain detection and configuration

#### Automated CI/CD Framework
- **GitHub Actions Integration**: Automated testing and validation
  - Matrix builds for different configurations and toolchains
  - Automated hardware-in-the-loop testing capabilities
  - Comprehensive test reporting and artifact generation

#### Configurable Feature Optimization
- **Fine-Grained Feature Selection**: Minimal footprint builds
  - Automatic dependency resolution for selected features
  - Size optimization profiles (minimal, default, full)
  - Build validation for feature selection and optimization

### 🧪 Comprehensive Testing and Validation

#### Complete Test Suite
- **Unit Tests**: All new components fully tested
  - Event groups with all API functions and edge cases
  - Stream buffers with various message sizes and patterns
  - Memory pools with allocation patterns and stress scenarios
  - Multi-core tests for task distribution and synchronization

#### Integration Testing
- **Cross-Component Validation**: Component interaction testing
  - Event groups with multi-core task coordination
  - Stream buffers with memory pools integration
  - Profiling and tracing with all synchronization primitives
  - Cross-component error handling and recovery validation

#### Performance Testing
- **Regression Prevention**: Automated performance benchmarks
  - Context switch timing validation with new features
  - Memory allocation performance testing
  - Real-time compliance validation and certification

#### Hardware Validation
- **Real Hardware Testing**: Comprehensive RP2040 validation
  - Multi-core functionality with real hardware timing
  - Memory protection and stack overflow detection
  - Extended runtime stability testing and validation

### 📚 Documentation and Examples

#### Comprehensive API Documentation
- **Complete Reference**: All new features documented
  - Event groups with detailed usage examples
  - Stream buffer APIs with performance characteristics
  - Memory pool documentation with best practices
  - Multi-core programming guide with examples

#### Professional Examples
- **Production-Quality Demonstrations**: Real-world usage patterns
  - Event group example with multi-event coordination
  - Stream buffer example with efficient data streaming
  - Multi-core example with load balancing and IPC
  - Debugging and profiling example with real-time analysis

#### Migration Documentation
- **Upgrade Support**: Comprehensive migration resources
  - Upgrade guide from v0.2.1 to v0.3.0
  - New configuration options and their impact
  - Troubleshooting guide for migration issues
  - Performance tuning guide for new features

### 🔧 Production Scripts and Tools

#### Build System
- **Production-Ready Scripts**: Complete build automation
  - `scripts/build.sh`: Unified build script with configuration profiles
  - `scripts/test.sh`: Comprehensive test runner
  - `scripts/validate.sh`: System validation and quality checks
  - `scripts/menuconfig.py`: Enhanced configuration system

#### Configuration Profiles
- **Optimized Configurations**: Pre-configured build profiles
  - **Minimal**: Basic RTOS features only (8KB footprint)
  - **Default**: Standard feature set for typical applications
  - **Full**: All v0.3.0 features enabled for maximum capability

### ⚡ Performance Improvements

#### Real-Time Performance
- **Deterministic Behavior**: All operations have bounded execution time
- **Multi-Core Scaling**: Near-linear performance scaling on dual-core RP2040
- **Memory Efficiency**: Optimized memory usage with configurable limits
- **Interrupt Latency**: Minimized interrupt response time

#### Resource Optimization
- **Configurable Limits**: Fine-tuned resource allocation
- **Zero-Copy Operations**: Reduced memory copying for large data transfers
- **O(1) Algorithms**: Constant-time operations for critical paths
- **Hardware Acceleration**: Leveraged RP2040 hardware features

### 🔒 Security and Reliability

#### Memory Protection
- **MPU Integration**: Hardware-based memory protection
- **Stack Overflow Detection**: Enhanced protection mechanisms
- **Memory Leak Prevention**: Comprehensive tracking and validation
- **Buffer Overflow Protection**: Bounds checking and validation

#### System Reliability
- **Deadlock Prevention**: Runtime detection and resolution
- **Watchdog Integration**: Hardware-based fault tolerance
- **Health Monitoring**: Proactive system monitoring
- **Graceful Degradation**: Fault-tolerant operation modes

### 📊 Statistics and Monitoring

#### System Metrics
- **Real-Time Statistics**: Live system performance monitoring
- **Resource Usage**: Memory, CPU, and peripheral utilization
- **Performance Profiling**: Detailed execution time analysis
- **Health Indicators**: System health and reliability metrics

### 🔄 Migration from v0.2.1

#### Automatic Migration
- **Configuration Migration**: Automatic settings conversion
- **API Compatibility**: 100% backward compatibility maintained
- **Build System**: Seamless integration with existing projects
- **Documentation**: Step-by-step migration guide

#### New Features Available
- All v0.3.0 features are optional and can be enabled incrementally
- Existing applications continue to work without modification
- New features can be adopted gradually as needed
- Performance improvements are automatic with no code changes required

---

## [0.2.1] - 2025-07-21 - **ENHANCED DEVELOPER EXPERIENCE** 🎨

### CRITICAL PERFORMANCE FIX 🚨
- **Fixed Critical Real-Time Performance Issue**: Resolved O(n) blocking system that violated real-time requirements
  - **Root Cause**: Semaphore blocking objects were not properly initialized (always NULL)
  - **Performance Issue**: Unblocking tasks required O(n) traversal with interrupts disabled
  - **Real-Time Violation**: Non-deterministic timing could cause data loss in high-load scenarios
  - **Impact**: Critical sections could take 50-5000+ cycles depending on blocked task count
- **Implemented O(1) High-Performance Blocking System**:
  - **Priority-Ordered Insertion**: Tasks inserted in priority order for O(1) unblocking
  - **Doubly-Linked Lists**: O(1) removal operations using prev/next pointers
  - **Bounded Critical Sections**: Predictable timing characteristics (50-100 cycles)
  - **Real-Time Compliance**: Deterministic behavior suitable for real-time systems
- **Enhanced Semaphore Implementation**:
  - **Proper Blocking Integration**: Semaphores now correctly use blocking objects
  - **Direct Token Passing**: Tokens passed directly to unblocked tasks (no waste)
  - **Enhanced Error Reporting**: Specific error codes for semaphore operations
  - **Performance Monitoring**: Built-in performance statistics and validation

### Added
- **Menuconfig System**: Interactive terminal-based configuration system similar to Linux kernel menuconfig
  - Text-based ncurses interface with `make menuconfig`
  - Optional GUI interface with `make guiconfig`
  - Automatic generation of CMake and C header configuration files
  - Built-in validation and dependency checking
  - Support for configuration profiles and presets
- **Comprehensive Examples**: Four new professional examples demonstrating real-world usage patterns
  - **Hardware Interrupt Handling**: GPIO interrupt integration with RTOS-aware ISR implementation
  - **Multi-Task Communication**: Producer-consumer patterns, resource sharing, and task notifications
  - **Power Management**: Idle task hooks with Pico SDK sleep mode integration
  - **Performance Benchmarking**: Context switch timing, interrupt latency, and throughput measurement
- **Configurable System Tick Frequency**: Support for 100Hz, 250Hz, 500Hz, 1000Hz, 2000Hz, and custom rates
  - `pico_rtos_get_tick_rate_hz()` function to query current tick frequency
  - Compile-time validation of tick frequency values
  - Automatic timing calculations and period definitions
- **Enhanced Error Reporting System**: Comprehensive error codes with detailed context information
  - 60+ specific error codes organized by category (Task, Memory, Sync, System, Hardware, Config)
  - Error history tracking with circular buffer (configurable size)
  - Error statistics and callback support
  - Context capture including file, line, function, and timestamp
- **Debug Logging System**: Optional configurable logging with zero overhead when disabled
  - Multiple log levels (None, Error, Warning, Info, Debug)
  - Subsystem-specific filtering (Core, Task, Mutex, Queue, Timer, Memory, Semaphore)
  - Custom output functions and formatting options
  - Convenience macros for each subsystem
- **Enhanced Build System**: Extensive configuration options with validation
  - Feature toggles for all optional components
  - Resource limit configuration (max tasks, timers, stack sizes)
  - Logging configuration with level and subsystem control
  - Comprehensive validation with clear error messages

### Enhanced
- **Documentation**: Comprehensive guides for new features
  - **Error Code Reference**: Complete catalog with causes and solutions
  - **Logging System Guide**: Usage patterns and best practices
  - **Menuconfig Guide**: Interactive configuration system documentation
  - **Menuconfig Guide**: Interactive configuration system documentation
  - Updated User Guide and API Reference with v0.2.1 features
- **Build System Integration**: Seamless integration of menuconfig with existing CMake workflow
  - `make build` automatically uses menuconfig-generated configuration
  - Backward compatibility with traditional CMake configuration
  - Configuration file validation and dependency checking
- **Developer Experience**: Significantly improved configuration and debugging capabilities
  - Interactive configuration reduces manual CMake parameter management
  - Comprehensive error reporting aids in debugging and development
  - Optional logging provides detailed runtime information
  - Professional examples demonstrate best practices

### Fixed
- **CRITICAL: Real-Time Performance Issue**: Fixed O(n) blocking system that violated real-time requirements
  - **Semaphore Blocking Integration**: Fixed NULL blocking objects in semaphore initialization
  - **O(1) Unblocking**: Implemented priority-ordered lists for constant-time unblocking
  - **Bounded Critical Sections**: Eliminated variable-length critical sections
  - **Priority Preservation**: Ensured highest priority tasks are unblocked first
- **Menuconfig Symbol Access**: Fixed Kconfig symbol access using proper kconfiglib API
- **Configuration Generation**: Ensured configuration files are properly generated when loading defaults
- **Build Integration**: Improved integration between menuconfig system and CMake build

### Changed
- **Version Information**: Updated to v0.2.1 throughout codebase
- **Configuration Management**: Centralized configuration through menuconfig system
- **Documentation Structure**: Enhanced organization with new feature documentation

### Technical Details
- **Menuconfig Implementation**: Python-based system using kconfiglib
- **Configuration Files**: Automatic generation of `cmake_config.cmake` and `include/pico_rtos_config.h`
- **Error System**: Hierarchical error codes with optional history tracking
- **Logging System**: Compile-time configurable with runtime control
- **Examples**: Production-quality code demonstrating hardware integration patterns

### Performance Impact
- **CRITICAL IMPROVEMENT**: Fixed O(n) to O(1) unblocking performance
  - **Before Fix**: 50-5000+ cycles (variable, real-time violation)
  - **After Fix**: 50-100 cycles (bounded, real-time compliant)
  - **Improvement**: Up to 50x performance improvement with many blocked tasks
- **Zero Overhead**: Disabled features compile out completely
- **Minimal Overhead**: Enabled features add minimal runtime cost
- **Configuration Time**: Interactive configuration improves development workflow
- **Build Time**: Negligible impact on build performance

## [0.2.0] - 2025-07-18 - **PRODUCTION READY RELEASE** 🚀

### Added
- **Complete ARM Cortex-M0+ Context Switching**: Full assembly implementation with proper register handling
- **Idle Task Implementation**: Automatic idle task with power management and stack overflow checking
- **Priority Inheritance**: Proper priority inheritance in mutexes to prevent priority inversion
- **Stack Overflow Protection**: Dual canary-based stack overflow detection for all tasks
- **Memory Management Tracking**: Comprehensive memory allocation tracking with statistics
- **System Statistics**: Real-time system monitoring with `pico_rtos_get_system_stats()`
- **Task Priority Management**: Dynamic priority changes with `pico_rtos_task_set_priority()`
- **Interrupt Nesting Support**: Safe interrupt handling with deferred context switches
- **Enhanced Examples**: Professional LED blinking example with proper task priorities
- **Comprehensive System Test**: Complete validation test covering all RTOS features
- **Thread-Safe Operations**: All system calls are now properly protected with critical sections
- **Blocking/Unblocking Mechanism**: Complete implementation with timeout handling
- **Timer Overflow Handling**: Safe 32-bit timer overflow management
- **Enhanced Documentation**: Complete fix documentation and production readiness guide
- **Flashing and Testing Guide**: Comprehensive guide for hardware workflow and serial monitoring
- **Troubleshooting Documentation**: Detailed solutions for common issues and debugging
- **Contributing Guidelines**: Complete development and contribution workflow documentation
- **Documentation Index**: Organized documentation structure with navigation guide

### Fixed
- **Memory Management Consistency**: Fixed all memory allocation/deallocation to use tracked functions
- **Priority Inheritance Corruption**: Fixed priority inheritance to use task's original priority
- **Thread-Safety Issues**: Made all system calls thread-safe with proper critical sections
- **Stack Canary Placement**: Enhanced stack overflow detection with dual canaries
- **Context Switch Race Conditions**: Eliminated race conditions in task switching
- **Timer Callback Deadlocks**: Fixed timer callbacks to execute outside critical sections
- **Memory Leaks**: Eliminated all memory leaks in task creation/deletion
- **Include Path Issues**: Standardized all include paths for better maintainability
- **Task Execution Model**: Fixed task startup and execution flow
- **Scheduler State Corruption**: Protected all scheduler state modifications

### Changed
- **Task Structure**: Added `original_priority` field for proper priority inheritance
- **Mutex Structure**: Simplified mutex structure by removing redundant priority field
- **System Uptime**: Made `pico_rtos_get_uptime_ms()` thread-safe
- **Stack Initialization**: Enhanced stack setup with proper canary placement
- **Error Handling**: Added comprehensive input validation and error checking
- **Build System**: Updated CMakeLists.txt to include all new components
- **API Consistency**: Standardized return values and error handling across all functions

### Removed
- **Placeholder Comments**: Removed all "real implementation" placeholder comments
- **Unused Code**: Cleaned up obsolete task wrapper functions
- **Inconsistent Memory Calls**: Replaced all direct malloc/free calls with tracked versions

### Performance
- **Context Switch Time**: Optimized to ~50-100 CPU cycles on ARM Cortex-M0+
- **Memory Overhead**: Reduced to ~32 bytes per task + stack size
- **Interrupt Latency**: Minimized with proper interrupt nesting
- **CPU Utilization**: Added accurate idle time measurement

### Security
- **Stack Overflow Detection**: Comprehensive protection with dual canaries
- **Memory Bounds Checking**: Enhanced memory allocation validation
- **Priority Inversion Prevention**: Complete priority inheritance implementation
- **Race Condition Elimination**: Thread-safe operations throughout

### Testing
- **System Test Suite**: Comprehensive test covering all RTOS features
- **Priority Inheritance Test**: Validates proper priority inheritance behavior
- **Memory Management Test**: Validates leak-free operation
- **Synchronization Test**: Tests mutexes, semaphores, and queues
- **Stack Overflow Test**: Validates stack protection mechanisms
- **Performance Benchmarks**: Real-time performance measurement

## [0.1.0] - 2025-03-12

### Added
- Initial release of Pico-RTOS
- Core RTOS functionality including scheduler, task management, and system time
- Mutexes with proper ownership tracking and deadlock prevention
- Queues with blocking send/receive operations and timeout handling
- Semaphores with counting and binary semaphore support
- Software timers with one-shot and auto-reload functionality
- Critical section handling for thread safety
- Task delay functionality with timeout mechanism
- LED blinking example using task management
- Task synchronization example using semaphores
- Comprehensive API documentation
- Troubleshooting guide for common issues
- GitHub templates for issues and pull requests
- CMake build system with configurable options
- Unit tests for each RTOS component

### Fixed
- Task suspension/resume logic to work correctly with scheduler
- Timeout handling in blocking operations
- Proper stack allocation and initialization for tasks
- Mutex release logic to prevent priority inversion
- Queue management to properly handle full/empty conditions
- Timer handling to maintain accurate timing

### Changed
- Restructured project to follow standard C project layout
- Improved header organization with proper include guards
- Enhanced API design with consistent return values
- Standardized error handling across all functions
- Updated documentation to match implementation
- Updated documentation to match implementation

### Removed
- Removed incorrect task implementation that caused potential deadlocks
- Eliminated unsafe critical section handling
- Removed direct register access for better portability