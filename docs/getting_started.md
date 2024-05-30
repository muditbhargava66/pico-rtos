# Getting Started with Pico-RTOS

This guide will walk you through the steps to set up and start using Pico-RTOS on your Raspberry Pi Pico board.

## Prerequisites

- Raspberry Pi Pico board
- Pico SDK
- CMake
- GCC cross-compiler for ARM Cortex-M0+

## Installation

1. Clone the Pico-RTOS repository:
   ```
   git clone https://github.com/muditbhargava66/pico-rtos.git
   ```

2. Navigate to the project directory:
   ```
   cd pico-rtos
   ```

3. Create a build directory and navigate to it:
   ```
   mkdir build
   cd build
   ```

4. Configure the project using CMake:
   ```
   cmake ..
   ```

5. Build the project:
   ```
   make
   ```

6. Flash the generated binary to your Raspberry Pi Pico board.

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