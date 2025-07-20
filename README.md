<div align="center">

# Pico-RTOS

![Pico-RTOS Version](https://img.shields.io/badge/version-0.2.1-blue)
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

**Pico-RTOS is a production-ready, lightweight real-time operating system specifically designed for the Raspberry Pi Pico board. It provides a comprehensive set of APIs and primitives for multitasking, inter-task communication, synchronization, and timing, with professional-grade features like priority inheritance, stack overflow protection, memory management, and an intuitive menuconfig-style configuration system. Version 0.2.1 includes critical performance fixes that ensure true real-time compliance with O(1) unblocking and bounded critical sections.**

</div>

## Features

- **Task Management**
  - Preemptive multitasking with priority-based scheduling
  - Task creation, suspension, deletion, and priority management
  - Task delay, yield, and blocking functionality
  - Automatic idle task with power management

- **Inter-task Communication**
  - Queue-based messaging with timeout support
  - Send and receive with optional blocking
  - Priority-based task unblocking
  - Efficient memory management with tracking

- **Synchronization Primitives**
  - Mutexes with priority inheritance and deadlock prevention
  - Counting and binary semaphores
  - Robust timeout handling for all blocking operations
  - Complete blocking/unblocking mechanism

- **Memory Management**
  - Thread-safe dynamic memory allocation
  - Comprehensive memory usage tracking and statistics
  - Automatic cleanup of terminated tasks
  - Memory leak detection and prevention

- **Safety & Reliability**
  - Stack overflow protection with dual canaries
  - Thread-safe operations throughout
  - Interrupt nesting support with deferred context switches
  - Race condition elimination

- **System Monitoring**
  - Real-time system statistics and diagnostics
  - CPU usage monitoring with idle time measurement
  - Memory usage tracking and peak detection
  - Task state monitoring and debugging support

- **Timing Services**
  - System tick counter with overflow-safe handling
  - Software timers with one-shot and auto-reload modes
  - Accurate delay functions with timeout support
  - Timer callback execution outside critical sections

- **Enhanced Developer Experience (v0.2.1)**
  - Interactive menuconfig-style configuration interface
  - GUI configuration option with tkinter
  - Comprehensive examples demonstrating real-world integration patterns
  - Enhanced error reporting with 60+ specific error codes
  - Optional debug logging system with zero overhead when disabled
  - Configurable system tick frequency (100Hz to 2000Hz)

- **Configuration System**
  - Interactive menuconfig-style configuration interface
  - GUI configuration option with tkinter
  - Automatic generation of CMake and C header files
  - Built-in validation and dependency checking
  - Support for configuration profiles and presets

- **Core Features**
  - Production-ready ARM Cortex-M0+ context switching
  - Lightweight footprint optimized for the RP2040 processor
  - Professional-grade error handling and validation
  - Easy-to-use API with comprehensive documentation

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

## Comprehensive Examples (v0.2.1)

Pico-RTOS v0.2.1 includes four professional examples demonstrating real-world integration patterns:

### Hardware Interrupt Handling
```bash
make flash-hardware_interrupt
```
Demonstrates RTOS-aware GPIO interrupt handling with proper ISR implementation and safe communication between interrupt context and tasks using queues.

### Multi-Task Communication
```bash
make flash-task_communication
```
Shows three key communication patterns: queue-based producer-consumer, semaphore-based resource sharing, and lightweight task notifications.

### Power Management
```bash
make flash-power_management
```
Illustrates power optimization using idle task hooks, integration with Pico SDK sleep modes, and wake-up event handling.

### Performance Benchmarking
```bash
make flash-performance_benchmark
```
Provides comprehensive performance measurements including context switch timing, interrupt latency, and synchronization overhead.

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
├── config/                     # Configuration files (generated)
├── docs/                       # Comprehensive documentation
│   ├── release/               # Release-specific documentation
│   ├── api_reference.md       # Complete API documentation
│   ├── user_guide.md          # User guide and best practices
│   ├── error_codes.md         # Error code reference
│   ├── logging_guide.md       # Debug logging guide
│   └── menuconfig_guide.md    # Configuration system guide
├── examples/                   # Professional example applications
│   ├── led_blinking/          # Basic task management
│   ├── task_synchronization/  # Semaphore usage patterns
│   ├── task_communication/    # Inter-task communication
│   ├── hardware_interrupt/    # RTOS-aware interrupt handling
│   ├── power_management/      # Power optimization techniques
│   ├── performance_benchmark/ # Performance measurement
│   └── system_test/           # Comprehensive RTOS validation
├── include/                    # Public header files
│   ├── pico_rtos/             # Individual component headers
│   │   ├── config.h           # Configuration definitions
│   │   ├── error.h            # Error reporting system
│   │   ├── logging.h          # Debug logging system
│   │   └── [other headers]    # Core RTOS headers
│   └── pico_rtos.h            # Main RTOS header
├── scripts/                    # Build and utility scripts
│   ├── menuconfig.py          # Interactive configuration system
│   └── test_menuconfig.py     # Configuration system tests
├── src/                        # RTOS implementation
├── tests/                      # Unit and integration tests
├── Kconfig                     # Menuconfig definitions
├── Makefile                    # Build system with menuconfig
├── menuconfig.sh               # Configuration script
└── requirements.txt            # Python dependencies
├── src/
│   ├── mutex.c
│   ├── queue.c
│   ├── semaphore.c
│   ├── task.c
│   ├── timer.c
│   ├── core.c
│   ├── context_switch.c
│   ├── context_switch.S
│   └── blocking.c
├── tests/
│   ├── mutex_test.c
│   ├── queue_test.c
│   ├── semaphore_test.c
│   ├── task_test.c
│   └── timer_test.c
├── extern/
├── .gitignore
├── .gitmodules
├── CMakeLists.txt
├── CHANGELOG.md
├── TODO.md
├── FIXES_APPLIED.md
├── CODE_OF_CONDUCT.md
├── CONTRIBUTING.md
├── LICENSE
├── ROADMAP.md
└── README.md
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