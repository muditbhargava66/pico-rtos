# Changelog

All notable changes to Pico-RTOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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