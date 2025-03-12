# Changelog

All notable changes to Pico-RTOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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