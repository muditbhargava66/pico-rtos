<div align="center">

# Pico-RTOS

![Pico-RTOS Version](https://img.shields.io/badge/version-0.3.0-blue)
![Production Ready](https://img.shields.io/badge/status-production%20ready-brightgreen)
![Real-Time Compliant](https://img.shields.io/badge/real--time-compliant-success)
![License](https://img.shields.io/badge/license-MIT-green)
![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)
[![Contributors](https://img.shields.io/github/contributors/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/graphs/contributors)
[![Last Commit](https://img.shields.io/github/last-commit/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/commits/main)
[![Release](https://img.shields.io/github/v/release/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/releases)
[![Open Issues](https://img.shields.io/github/issues/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/issues)
[![Open PRs](https://img.shields.io/github/issues-pr/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/pulls)
[![GitHub stars](https://img.shields.io/github/stars/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/network/members)

**Pico-RTOS is a production-ready, lightweight real-time operating system specifically designed for the Raspberry Pi Pico board. Version 0.3.0 introduces advanced synchronization primitives (event groups, stream buffers), enhanced memory management with pools, multi-core SMP support, comprehensive debugging and profiling tools, and quality assurance features including deadlock detection and system health monitoring. Built for enterprise-grade reliability with extensive testing and validation.**

</div>

## Features

### Core RTOS Features
- **Task Management**: Preemptive multitasking with priority-based scheduling, task creation/deletion, and power management
- **Inter-task Communication**: Queue-based messaging with timeout support and priority-based unblocking
- **Synchronization Primitives**: Mutexes with priority inheritance, counting/binary semaphores, and robust timeout handling
- **Memory Management**: Thread-safe dynamic allocation, comprehensive tracking, and automatic cleanup
- **Timing Services**: System tick counter, software timers (one-shot/auto-reload), and accurate delay functions

### v0.3.0 Advanced Synchronization Primitives
- **Event Groups**: Efficient task synchronization using bit patterns with wait-for-any/all semantics
- **Stream Buffers**: High-performance byte stream communication with zero-copy optimization
- **Enhanced Timeouts**: Universal timeout support across all blocking operations

### v0.3.0 Enhanced Memory Management
- **Memory Pools**: Fixed-size block allocation with deterministic timing and comprehensive statistics
- **Memory Protection Unit (MPU)**: Hardware-assisted memory protection and access control
- **Advanced Tracking**: Detailed memory usage analytics and leak detection

### v0.3.0 Multi-Core Support
- **Symmetric Multi-Processing (SMP)**: True dual-core task scheduling with automatic load balancing
- **Inter-Core Synchronization**: Core-aware mutexes, semaphores, and communication primitives
- **Core Affinity**: Task pinning to specific cores for performance optimization
- **IPC Channels**: High-speed inter-processor communication

### v0.3.0 Debugging and Profiling
- **Runtime Task Inspection**: Live task state monitoring and stack usage analysis
- **Execution Profiling**: Function-level timing analysis with statistical reporting
- **System Tracing**: Comprehensive event logging for performance analysis
- **Enhanced Assertions**: Detailed error reporting with context information

### v0.3.0 System Extensions
- **I/O Abstraction Layer**: Unified interface for hardware peripherals
- **High-Resolution Timers**: Microsecond-precision timing for critical applications
- **Universal Timeouts**: Consistent timeout handling across all RTOS components

### v0.3.0 Quality Assurance
- **Deadlock Detection**: Automatic detection and reporting of potential deadlocks
- **System Health Monitoring**: Continuous monitoring of system resources and performance
- **Hardware Watchdog Integration**: Automatic system recovery from failures
- **Alert System**: Configurable thresholds and notifications for system events

### v0.3.0 Compatibility and Migration
- **Backward Compatibility**: Full compatibility with v0.2.1 applications
- **Migration Warnings**: Helpful warnings for deprecated APIs with migration guidance
- **Legacy Support**: Continued support for existing applications during transition

## Getting Started

To get started with Pico-RTOS, follow these steps:

### 1. Clone the Repository

```bash
git clone https://github.com/muditbhargava66/pico-rtos.git
cd pico-rtos
```

### 2. Set Up the Pico SDK

You have three options for setting up the Pico SDK:

#### Option A: Use the Setup Script (Recommended)

Run the provided setup script, which will automatically set up the Pico SDK as a submodule:

```bash
chmod +x setup-pico-sdk.sh
./setup-pico-sdk.sh
```

#### Option B: Manual Submodule Setup

Alternatively, you can manually set up the submodule:

```bash
# Add the Pico SDK as a submodule
git submodule add -f -b master https://github.com/raspberrypi/pico-sdk.git extern/pico-sdk

# Initialize and update submodules
git submodule update --init --recursive

# Copy the SDK import script to the project root
cp extern/pico-sdk/external/pico_sdk_import.cmake .
```

#### Option C: Use an Existing SDK Installation

If you already have the Pico SDK installed elsewhere, you can set the `PICO_SDK_PATH` environment variable:

```bash
# Linux/macOS
export PICO_SDK_PATH=/path/to/your/pico-sdk

# Windows
set PICO_SDK_PATH=C:\path\to\your\pico-sdk
```

### 3. Configure and Build the Project

Pico-RTOS now includes a menuconfig-style configuration system for easy customization:

#### Option A: Using Menuconfig (Recommended)

```bash
# Install Python dependencies
pip install -r requirements.txt

# Configure interactively
make menuconfig

# Build with your configuration
make build
```

#### Option B: Using GUI Configuration

```bash
# Use graphical configuration interface
make guiconfig

# Build with your configuration
make build
```

#### Option C: Traditional CMake Build

```bash
mkdir build
cd build
cmake ..
make
```

#### Quick Start

For a complete setup from scratch:

```bash
# Set up everything and build with defaults
make quick-start
```

### 4. Flash and Run

Connect your Raspberry Pi Pico board to your computer while holding the BOOTSEL button. It will appear as a USB drive. Copy the desired `.uf2` file to the drive:

```bash
# For example, to flash the LED blinking example:
cp examples/led_blinking/led_blinking.uf2 /Volumes/RPI-RP2/  # macOS
# or
cp examples/led_blinking/led_blinking.uf2 /media/username/RPI-RP2/  # Linux
# or
copy examples\led_blinking\led_blinking.uf2 E:\  # Windows
```

## Usage Examples

### Creating Tasks

```c
void my_task(void *param) {
    while (1) {
        // Do something
        pico_rtos_task_delay(100);  // Delay for 100ms
    }
}

// In your main function:
pico_rtos_task_t task;
pico_rtos_task_create(&task, "My Task", my_task, NULL, 256, 1);
```

### Using Mutexes

```c
pico_rtos_mutex_t mutex;
pico_rtos_mutex_init(&mutex);

// In your task:
if (pico_rtos_mutex_lock(&mutex, PICO_RTOS_WAIT_FOREVER)) {
    // Critical section - protected access to shared resources
    
    pico_rtos_mutex_unlock(&mutex);
}
```

### Inter-task Communication with Queues

```c
pico_rtos_queue_t queue;
uint8_t queue_buffer[5 * sizeof(int)];
pico_rtos_queue_init(&queue, queue_buffer, sizeof(int), 5);

// In sender task:
int data = 42;
pico_rtos_queue_send(&queue, &data, PICO_RTOS_WAIT_FOREVER);

// In receiver task:
int received;
if (pico_rtos_queue_receive(&queue, &received, 100)) {
    // Process received data (timeout of 100ms)
}
```

### Using Timers

```c
void timer_callback(void *param) {
    // Timer expired, do something
}

pico_rtos_timer_t timer;
pico_rtos_timer_init(&timer, "My Timer", timer_callback, NULL, 1000, true);
pico_rtos_timer_start(&timer);
```

## Comprehensive Examples (v0.3.0)

Pico-RTOS v0.3.0 includes 11 professional examples demonstrating real-world integration patterns:

### Core Examples
- **LED Blinking**: Basic task management and GPIO control
- **Task Synchronization**: Advanced semaphore and mutex usage patterns
- **Task Communication**: Queue-based producer-consumer and notification systems
- **Hardware Interrupt**: RTOS-aware GPIO interrupt handling with safe ISR communication

### Advanced v0.3.0 Examples
- **Event Group Coordination**: Complex task synchronization using event groups
- **Stream Buffer Data Streaming**: High-performance data streaming with zero-copy optimization
- **Multicore Load Balancing**: SMP scheduling and inter-core communication
- **Performance Benchmark**: Comprehensive timing analysis and performance measurement
- **Power Management**: Advanced power optimization with multi-core considerations
- **System Test**: Complete RTOS validation and stress testing
- **Debugging & Profiling Analysis**: Real-time system monitoring and performance profiling

### Building and Running Examples
```bash
# Build all examples
make examples

# Build specific example
make led_blinking
make multicore_load_balancing
make debugging_profiling_analysis

# Flash to device (example)
cp build/examples/led_blinking/led_blinking.uf2 /Volumes/RPI-RP2/
```

## Documentation

The Pico-RTOS documentation is available in the `docs/` directory:

- [Getting Started](docs/getting_started.md): Step-by-step guide to set up and start using Pico-RTOS
- [Flashing and Testing Guide](docs/flashing_and_testing.md): Complete guide for flashing firmware and monitoring output
- [User Guide](docs/user_guide.md): Comprehensive guide on using Pico-RTOS in your projects
- [API Reference](docs/api_reference.md): Detailed information about the Pico-RTOS API and its usage
- [Troubleshooting](docs/troubleshooting.md): Solutions to common issues and debugging tips
- [Contributing Guidelines](docs/contributing.md): Information on how to contribute to the Pico-RTOS project

## Project Structure

```
pico-rtos/
├── cmake/                      # CMake modules and configuration
├── config/                     # Configuration files (generated)
├── docs/                       # Comprehensive documentation
│   ├── api_reference.md       # Complete API documentation
│   ├── user_guide.md          # User guide and best practices
│   ├── getting_started.md     # Getting started guide
│   ├── flashing_and_testing.md # Hardware testing guide
│   ├── troubleshooting.md     # Common issues and solutions
│   └── contributing.md        # Contribution guidelines
├── examples/                   # Professional example applications (11 total)
│   ├── led_blinking/          # Basic task management
│   ├── task_synchronization/  # Advanced synchronization patterns
│   ├── task_communication/    # Inter-task communication
│   ├── hardware_interrupt/    # RTOS-aware interrupt handling
│   ├── event_group_coordination/ # Event group synchronization
│   ├── stream_buffer_data_streaming/ # High-performance streaming
│   ├── multicore_load_balancing/ # SMP and load balancing
│   ├── performance_benchmark/ # Performance measurement
│   ├── power_management/      # Power optimization
│   ├── system_test/           # Comprehensive RTOS validation
│   └── debugging_profiling_analysis/ # System monitoring
├── extern/                     # External dependencies
│   └── pico-sdk/              # Pico SDK submodule
├── include/                    # Public header files
│   ├── pico_rtos/             # Individual component headers
│   │   ├── config.h           # Configuration definitions
│   │   ├── task.h             # Task management
│   │   ├── mutex.h            # Mutex synchronization
│   │   ├── queue.h            # Queue communication
│   │   ├── semaphore.h        # Semaphore primitives
│   │   ├── timer.h            # Timer services
│   │   ├── event_group.h      # Event group synchronization
│   │   ├── stream_buffer.h    # Stream buffer communication
│   │   ├── memory_pool.h      # Memory pool management
│   │   ├── smp.h              # Multi-core support
│   │   ├── debug.h            # Debugging tools
│   │   ├── profiler.h         # Performance profiling
│   │   ├── trace.h            # System tracing
│   │   ├── health.h           # System health monitoring
│   │   ├── watchdog.h         # Watchdog integration
│   │   ├── alerts.h           # Alert system
│   │   ├── logging.h          # Debug logging
│   │   ├── timeout.h          # Universal timeouts
│   │   ├── hires_timer.h      # High-resolution timers
│   │   ├── io.h               # I/O abstraction
│   │   ├── mpu.h              # Memory protection
│   │   ├── deadlock.h         # Deadlock detection
│   │   ├── compatibility.h    # Backward compatibility
│   │   ├── deprecation.h      # Deprecation warnings
│   │   └── error.h            # Error handling
│   └── pico_rtos.h            # Main RTOS header
├── scripts/                    # Build and utility scripts
├── src/                        # RTOS implementation
│   ├── core/                  # Core RTOS functionality
│   ├── sync/                  # Synchronization primitives
│   ├── memory/                # Memory management
│   ├── multicore/             # Multi-core support
│   ├── debug/                 # Debugging and profiling
│   ├── system/                # System extensions
│   └── compat/                # Compatibility layer
├── tests/                      # Unit and integration tests (8 total)
│   ├── mutex_test.c           # Mutex functionality tests
│   ├── queue_test.c           # Queue communication tests
│   ├── task_test.c            # Task management tests
│   ├── event_group_test.c     # Event group tests
│   ├── memory_pool_test.c     # Memory pool tests
│   ├── comprehensive_event_group_test.c # Advanced event group tests
│   ├── multicore_comprehensive_test.c # Multi-core tests
│   ├── integration_component_interactions_test.c # Integration tests
│   ├── CMakeLists_comprehensive.txt # Comprehensive test configuration
│   └── README.md              # Test documentation
├── .gitignore                 # Git ignore rules
├── .gitmodules                # Git submodule configuration
├── CMakeLists.txt             # Main CMake configuration
├── CHANGELOG.md               # Version history and changes
├── LICENSE                    # MIT License
├── README.md                  # This file
├── RELEASE_NOTES_v0.3.0.md    # v0.3.0 release notes
└── ROADMAP.md                 # Development roadmap
```

## Contributing

Contributions to Pico-RTOS are welcome! If you find any issues or have suggestions for improvements, please open an issue or submit a pull request. For more information, see the [Contributing Guidelines](docs/contributing.md).

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the development plan and upcoming features.

## License

Pico-RTOS is released under the [MIT License](LICENSE).

## Acknowledgements

Pico-RTOS was inspired by the design and concepts of other popular real-time operating systems. We would like to acknowledge their contributions to the embedded systems community and the excellent documentation provided by the Raspberry Pi Foundation for the Pico platform.

<div align="center">

---
⭐️ Star the repo and consider contributing!  
  
📫 **Contact**: [@muditbhargava66](https://github.com/muditbhargava66)
🐛 **Report Issues**: [Issue Tracker](https://github.com/muditbhargava66/pico-rtos/issues)
  
© 2025 Mudit Bhargava. [MIT License](LICENSE)  
<!-- Copyright symbol using HTML entity for better compatibility -->
</div>