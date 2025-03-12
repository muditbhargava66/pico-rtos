<div align="center">

# Pico-RTOS

![Pico-RTOS Version](https://img.shields.io/badge/version-0.1.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)
[![Contributors](https://img.shields.io/github/contributors/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/graphs/contributors)
[![Last Commit](https://img.shields.io/github/last-commit/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/commits/main)
[![Release](https://img.shields.io/github/v/release/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/releases)
[![Open Issues](https://img.shields.io/github/issues/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/issues)
[![Open PRs](https://img.shields.io/github/issues-pr/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/pulls)
[![GitHub stars](https://img.shields.io/github/stars/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/muditbhargava66/pico-rtos)](https://github.com/muditbhargava66/pico-rtos/network/members)

**Pico-RTOS is a lightweight real-time operating system specifically designed for the Raspberry Pi Pico board. It provides a set of APIs and primitives for multitasking, inter-task communication, synchronization, and timing, allowing you to develop efficient and responsive embedded applications.**

</div>

## Features

- **Task Management**
  - Preemptive multitasking with priority-based scheduling
  - Task creation, suspension, and deletion
  - Task delay and yield functionality

- **Inter-task Communication**
  - Queue-based messaging with timeout support
  - Send and receive with optional blocking
  - Efficient memory management

- **Synchronization Primitives**
  - Mutexes with ownership tracking and deadlock prevention
  - Counting and binary semaphores
  - Robust timeout handling for all blocking operations

- **Timing Services**
  - System tick counter with millisecond precision
  - Software timers with one-shot and auto-reload modes
  - Accurate delay functions

- **Core Features**
  - Lightweight footprint optimized for the RP2040 processor
  - Efficient context switching
  - Critical section management
  - Easy-to-use API for task management and synchronization

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

### 3. Build the Project

```bash
mkdir build
cd build
cmake ..
make
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

## Documentation

The Pico-RTOS documentation is available in the `docs/` directory:

- [API Reference](docs/api_reference.md): Detailed information about the Pico-RTOS API and its usage.
- [User Guide](docs/user_guide.md): Comprehensive guide on using Pico-RTOS in your projects.
- [Getting Started](docs/getting_started.md): Step-by-step guide to set up and start using Pico-RTOS.
- [Troubleshooting](docs/troubleshooting.md): Solutions to common issues.
- [Contributing Guidelines](docs/contributing.md): Information on how to contribute to the Pico-RTOS project.

## Project Structure

```
pico-rtos/
├── .github/
│   ├── ISSUE_TEMPLATE/
│   │   ├── bug_report.md
│   │   └── feature_request.md
│   └── PULL_REQUEST_TEMPLATE.md
├── docs/
│   ├── api_reference.md
│   ├── contributing.md
│   ├── troubleshooting.md
│   ├── getting_started.md
│   └── user_guide.md
├── examples/
│   ├── led_blinking/
│   │   ├── CMakeLists.txt
│   │   └── main.c
│   └── task_synchronization/
│       ├── CMakeLists.txt
│       └── main.c
├── include/
│   ├── pico_rtos/
│   │   ├── mutex.h
│   │   ├── queue.h
│   │   ├── semaphore.h
│   │   ├── task.h
│   │   └── timer.h
│   └── pico_rtos.h
├── src/
│   ├── mutex.c
│   ├── queue.c
│   ├── semaphore.c
│   ├── task.c
│   ├── timer.c
│   └── core.c
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