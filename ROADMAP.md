# Pico-RTOS Roadmap

This document outlines the planned development roadmap for Pico-RTOS across future versions.

## Version 0.2.0 (Q2 2025)

### Core Functionality
- [ ] Complete proper context switching for ARM Cortex-M0+
- [ ] Implement thread-safe dynamic memory allocation
- [ ] Add configurable system tick frequency
- [ ] Support for task priorities with preemption
- [ ] Implement task yield functionality

### Synchronization
- [ ] Enhance mutex with priority inheritance to prevent priority inversion
- [ ] Add recursive mutex support
- [ ] Implement task notification system (lightweight alternative to semaphores)
- [ ] Add support for mutex timeout handling

### System Services
- [ ] Add idle task hook for power management
- [ ] Implement runtime statistics collection
- [ ] Integrate with Pico SDK sleep modes
- [ ] Add system uptime tracking with rollover handling

### Examples & Documentation
- [ ] Add example for hardware interrupt handling
- [ ] Create example for multi-task communication patterns
- [ ] Improve getting started documentation
- [ ] Add memory management guide

## Version 0.3.0 (Q3 2025)

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

## Version 0.4.0 (Q4 2025)

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

## Version 1.0.0 (Q1 2026)

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