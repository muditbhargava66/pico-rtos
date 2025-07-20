# Pico-RTOS Roadmap

This document outlines the planned development roadmap for Pico-RTOS across future versions.

## Version 0.2.0 (COMPLETED - January 2025) ✅

### Core Functionality
- [x] Complete proper context switching for ARM Cortex-M0+
- [x] Implement thread-safe dynamic memory allocation
- [x] Add configurable system tick frequency
- [x] Support for task priorities with preemption
- [x] Implement task yield functionality
- [x] Add idle task implementation
- [x] Implement stack overflow protection
- [x] Add interrupt nesting support

### Synchronization
- [x] Enhance mutex with priority inheritance to prevent priority inversion
- [x] Add recursive mutex support
- [x] Implement task notification system (lightweight alternative to semaphores)
- [x] Add support for mutex timeout handling
- [x] Complete blocking/unblocking mechanism

### System Services
- [x] Add idle task hook for power management
- [x] Implement runtime statistics collection
- [x] Integrate with Pico SDK sleep modes
- [x] Add system uptime tracking with rollover handling
- [x] Add comprehensive memory tracking
- [x] Implement system diagnostics and monitoring

### Examples & Documentation
- [ ] Add example for hardware interrupt handling *(moved to v0.2.1)*
- [ ] Create example for multi-task communication patterns *(moved to v0.2.1)*
- [x] Improve getting started documentation
- [x] Add memory management guide
- [x] Create comprehensive flashing and testing guide
- [x] Add detailed troubleshooting documentation
- [x] Implement contributing guidelines

## Version 0.2.1 (COMPLETED - July 2025) ✅

### Examples & Documentation
- [x] Add example for hardware interrupt handling
- [x] Create example for multi-task communication patterns
- [x] Add power management example using idle task hooks
- [x] Create performance benchmarking example

### Enhanced Developer Experience
- [x] Add configurable system tick frequency options
- [x] Enhance error reporting with more detailed codes (60+ error codes)
- [x] Add optional debug logging system with subsystem filtering
- [x] Improve build system with extensive configuration options
- [x] Interactive menuconfig system with text and GUI interfaces
- [x] Comprehensive documentation with error code reference and logging guide
- [x] Automated testing and validation framework

## Version 0.3.0 (Q2 2025)

### Core Functionality
- [ ] Implement event groups for multi-event synchronization
- [ ] Add stream buffers for efficient message passing
- [ ] Support for task local storage
- [ ] Implement task deletion and cleanup

### Memory Management
- [ ] Add memory pools for fixed-size allocations
- [ ] Implement memory protection if hardware supports it
- [ ] Add memory usage statistics and tracking
- [ ] Support for stack overflow detection

### Debugging & Profiling
- [ ] Add runtime task state inspection
- [ ] Implement execution time profiling
- [ ] Add trace buffer for system events
- [ ] Support for configurable assertion handling

### Extensions
- [ ] Basic thread-safe I/O abstraction
- [ ] Configurable logging system with multiple levels
- [ ] Support for software timers with higher resolution
- [ ] Add timeouts to all blocking operations

## Version 0.4.0 (Q3 2025)

### Advanced Features
- [ ] Implement multi-core support for RP2040
- [ ] Add message queues with structured messages
- [ ] Support for delayed task startup
- [ ] Implement task suspension with timeout

### Peripheral Integration
- [ ] Thread-safe GPIO handling
- [ ] Integrate with Pico PIO for custom peripherals
- [ ] Support for thread-safe ADC operations
- [ ] DMA support with proper synchronization

### System Monitoring
- [ ] Add watchdog integration
- [ ] Implement deadlock detection
- [ ] Add CPU usage monitoring
- [ ] Support for system health metrics

### Build System
- [ ] Enhanced CMake integration
- [ ] Support for different toolchains
- [ ] Optimize for code size and performance
- [ ] Configurable feature set to minimize footprint

## Version 1.0.0 (Q4 2025)

### Stability & Performance
- [ ] Complete MISRA C compliance
- [ ] Thorough performance optimization
- [ ] Comprehensive test coverage
- [ ] Validate real-time determinism

### Features
- [ ] File system abstraction
- [ ] Basic networking support
- [ ] Interface with common sensors
- [ ] Support for external memory management

### Documentation & Support
- [ ] Complete API reference
- [ ] Comprehensive user manual
- [ ] Migration guides from other RTOSes
- [ ] Design pattern documentation

### Community
- [ ] Public contribution guidelines
- [ ] Regular release schedule
- [ ] LTS (Long-Term Support) policy
- [ ] Community forum or discussion platform

## Future Considerations (Post 1.0)

### Advanced Features
- Command-line interface for debugging
- RTOS-aware GDB debugging
- Remote monitoring capabilities
- OTA update capabilities

### Ecosystem
- IDE integration tools
- Visual task and resource monitoring
- Configuration wizard
- Project template generator

### Standards Compliance
- POSIX thread API compatibility layer
- Safety certification preparation
- Formal verification of critical sections

### Hardware Support
- Expanded board support beyond Pico
- Support for other ARM Cortex microcontrollers
- Abstraction layer for portability