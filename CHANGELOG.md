# Changelog

All notable changes to Pico-RTOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

### Removed
- Removed incorrect task implementation that caused potential deadlocks
- Eliminated unsafe critical section handling
- Removed direct register access for better portability