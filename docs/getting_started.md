# Getting Started with Pico-RTOS

This guide will walk you through the steps to set up and start using Pico-RTOS on your Raspberry Pi Pico board.

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

### 4. Flash the generated binary to your Raspberry Pi Pico board.

## Usage

1. Include the necessary Pico-RTOS header files in your application code.
2. Initialize the Pico-RTOS components you want to use (e.g., tasks, queues, semaphores).
3. Create and start tasks using the Pico-RTOS API.
4. Build and flash your application to the Raspberry Pi Pico board.

For more detailed information on using the Pico-RTOS API, refer to the [User Guide](user_guide.md) and [API Reference](api_reference.md).

## Examples

The Pico-RTOS repository includes example projects in the `examples/` directory. These examples demonstrate various features and usage patterns of the RTOS.

To build and run an example project:

1. Navigate to the example project directory:
   ```
   cd examples/led_blinking
   ```

2. Follow the installation steps mentioned above to build and flash the example project.

## Troubleshooting

If you encounter any issues or have questions, please refer to the [Troubleshooting Guide](troubleshooting.md) or open an issue on the GitHub repository.

## Contributing

Contributions to Pico-RTOS are welcome! Please refer to the [Contributing Guidelines](contributing.md) for more information on how to contribute to the project.

Happy coding with Pico-RTOS!

---