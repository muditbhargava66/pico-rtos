# Pico-RTOS

Pico-RTOS is a lightweight real-time operating system specifically designed for the Raspberry Pi Pico board. It provides a set of APIs and primitives for multitasking, inter-task communication, synchronization, and timing, allowing you to develop efficient and responsive embedded applications.

## Features

- Preemptive multitasking with priority-based scheduling
- Inter-task communication using queues and semaphores
- Resource sharing and synchronization with mutexes
- Software timers for periodic and one-shot events
- Lightweight and efficient implementation
- Easy-to-use API for task management and synchronization

## Getting Started

To get started with Pico-RTOS, follow these steps:

1. Clone the Pico-RTOS repository:
   ```
   git clone https://github.com/muditbhargava66/pico-rtos.git
   ```

2. Set up the development environment:
   - Install the Pico SDK
   - Install CMake and a GCC cross-compiler for ARM Cortex-M0+

3. Build the Pico-RTOS library:
   ```
   cd pico-rtos
   mkdir build
   cd build
   cmake ..
   make
   ```

4. Create your application:
   - Include the necessary Pico-RTOS header files
   - Initialize the RTOS components (tasks, queues, semaphores, etc.)
   - Implement your application logic

5. Build and flash your application:
   ```
   cd your-app-directory
   mkdir build
   cd build
   cmake ..
   make
   ```
   - Flash the generated binary to your Raspberry Pi Pico board

For detailed instructions and examples, refer to the [Getting Started Guide](docs/getting_started.md).

## Documentation

The Pico-RTOS documentation is available in the `docs/` directory:

- [API Reference](docs/api_reference.md): Detailed information about the Pico-RTOS API and its usage.
- [User Guide](docs/user_guide.md): Comprehensive guide on using Pico-RTOS in your projects.
- [Getting Started](docs/getting_started.md): Step-by-step guide to set up and start using Pico-RTOS.
- [Contributing Guidelines](docs/contributing.md): Information on how to contribute to the Pico-RTOS project.

## Examples

The `examples/` directory contains sample projects that demonstrate the usage of Pico-RTOS:

- `led_blinking`: Simple LED blinking example using tasks and delays.
- `task_synchronization`: Demonstration of task synchronization using semaphores.

Feel free to explore and modify these examples to learn more about Pico-RTOS.

## Contributing

Contributions to Pico-RTOS are welcome! If you find any issues or have suggestions for improvements, please open an issue or submit a pull request. For more information, see the [Contributing Guidelines](docs/contributing.md).

## License

Pico-RTOS is released under the [MIT License](LICENSE).

## Acknowledgements

Pico-RTOS was inspired by the design and concepts of other popular real-time operating systems. We would like to acknowledge their contributions to the embedded systems community.

## Contact

If you have any questions, suggestions, or feedback, please feel free to contact us at [muditbhargava66](https://github.com/muditbhargava66).

Happy coding with Pico-RTOS!

---