# Getting Started with Pico-RTOS v0.3.0

This guide will walk you through the steps to set up and start using **Pico-RTOS v0.3.0 "Advanced Synchronization & Multi-Core"** on your Raspberry Pi Pico board.

## 🚀 Production Ready v0.3.0

Pico-RTOS v0.3.0 is **production-ready** with enterprise-grade features including:
- **Advanced Synchronization**: Event Groups, Stream Buffers, Enhanced Mutexes
- **Multi-Core Support**: SMP scheduler, load balancing, inter-core communication
- **Enhanced Memory Management**: Memory Pools, MPU support, advanced statistics
- **Debugging & Profiling**: Runtime inspection, execution profiling, system tracing
- **Production Quality**: Deadlock detection, health monitoring, watchdog integration
- **100% Backward Compatibility**: Seamless upgrade from v0.2.1

## Prerequisites

- Raspberry Pi Pico board
- Pico SDK
- CMake
- GCC cross-compiler for ARM Cortex-M0+

## Installation

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
git submodule add -b master https://github.com/raspberrypi/pico-sdk.git extern/pico-sdk

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

### 4. Flash and Test

After building, you'll have several executables ready to flash:

- **Examples**: `led_blinking.uf2`, `task_synchronization.uf2`, `system_test.uf2`
- **Tests**: `mutex_test.uf2`, `queue_test.uf2`, `semaphore_test.uf2`, `task_test.uf2`, `timer_test.uf2`

For detailed flashing and testing instructions, see the [Flashing and Testing Guide](flashing_and_testing.md).

## Usage

1. Include the necessary Pico-RTOS header files in your application code.
2. Initialize the Pico-RTOS components you want to use (e.g., tasks, queues, semaphores).
3. Create and start tasks using the Pico-RTOS API.
4. Build and flash your application to the Raspberry Pi Pico board.

For more detailed information on using the Pico-RTOS API, refer to the [User Guide](user_guide.md) and [API Reference](api_reference.md).

## Examples

The Pico-RTOS repository includes comprehensive example projects in the `examples/` directory:

### Available Examples

1. **LED Blinking** (`examples/led_blinking/`) - Professional LED control with proper task priorities and mutex synchronization
2. **Task Synchronization** (`examples/task_synchronization/`) - Demonstrates semaphore usage between tasks
3. **System Test** (`examples/system_test/`) - Comprehensive test covering all RTOS features including:
   - Priority inheritance validation
   - Memory management testing
   - Queue and semaphore operations
   - System statistics monitoring
   - Stack overflow protection

### Running Examples

To build and run an example project:

1. Build all examples:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

2. Flash the desired example:
   ```bash
   # For comprehensive system test (recommended first test):
   cp examples/system_test/system_test.uf2 /path/to/pico/drive/
   
   # For LED blinking example:
   cp examples/led_blinking/led_blinking.uf2 /path/to/pico/drive/
   ```

3. Connect to serial output to see the results (see [Flashing and Testing Guide](flashing_and_testing.md)):
   - Baud rate: 115200
   - Use `screen`, `minicom`, or the provided helper script

## Troubleshooting

If you encounter any issues or have questions, please refer to the [Troubleshooting Guide](troubleshooting.md) or open an issue on the GitHub repository.

## Contributing

Contributions to Pico-RTOS are welcome! Please refer to the [Contributing Guidelines](contributing.md) for more information on how to contribute to the project.

Happy coding with Pico-RTOS!

---