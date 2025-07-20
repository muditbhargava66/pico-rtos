# Pico-RTOS Menuconfig System Guide

This guide explains how to use the menuconfig-style configuration system for Pico-RTOS, which provides an interactive terminal-based interface for configuring build options.

## Overview

The Pico-RTOS menuconfig system provides:

- **Interactive Configuration**: Terminal-based menu system similar to Linux kernel configuration
- **GUI Option**: Optional graphical interface using tkinter
- **Automatic Generation**: Generates CMake and C header configuration files
- **Validation**: Built-in validation of configuration options
- **Presets**: Default configurations and the ability to save/load configurations

## Installation

### Prerequisites

1. **Python 3.6+** - Required for the configuration system
2. **kconfiglib** - Python library for Kconfig parsing

### Quick Setup

```bash
# Install Python dependencies
pip install -r requirements.txt

# Or install manually
pip install kconfiglib

# Make scripts executable (Linux/macOS)
chmod +x menuconfig.sh
```

## Usage

### Method 1: Using the Makefile (Recommended)

```bash
# Interactive text-based configuration
make menuconfig

# GUI-based configuration (requires tkinter)
make guiconfig

# Load default configuration
make defconfig

# Show current configuration
make showconfig

# Save current config as default
make savedefconfig
```

### Method 2: Using the Shell Script

```bash
# Text-based menuconfig
./menuconfig.sh

# GUI-based configuration
./menuconfig.sh --gui

# Show current configuration
./menuconfig.sh --show

# Load defaults
./menuconfig.sh --defaults
```

### Method 3: Direct Python Script

```bash
# Basic usage
python3 scripts/menuconfig.py

# With custom files
python3 scripts/menuconfig.py --config-file my_config.conf --cmake-file my_cmake.cmake

# GUI mode
python3 scripts/menuconfig.py --gui

# Show configuration only
python3 scripts/menuconfig.py --show-config
```

## Configuration Menu Structure

### Build Options
- **Build Examples**: Include example projects in build
- **Build Tests**: Include unit tests in build  
- **Enable Debug**: Enable debug output and assertions
- **Enable Install**: Enable CMake installation targets

### System Timing
- **System Tick Frequency**: Choose from predefined rates (100Hz to 2000Hz)
  - 100 Hz (10ms) - Low overhead, coarse timing
  - 250 Hz (4ms) - Balanced performance
  - 500 Hz (2ms) - Good timing resolution
  - 1000 Hz (1ms) - Default, excellent timing
  - 2000 Hz (0.5ms) - High resolution, higher overhead
- **Custom Tick Rate**: Option to specify custom frequency

### System Resources
- **Maximum Tasks**: Number of concurrent tasks (1-64)
- **Maximum Timers**: Number of software timers (1-32)
- **Default Task Stack Size**: Default stack allocation (256-8192 bytes)
- **Idle Task Stack Size**: Stack size for idle task (128-1024 bytes)

### Feature Configuration
- **Stack Overflow Checking**: Enable automatic stack overflow detection
- **Memory Usage Tracking**: Track allocations and provide statistics
- **Runtime Statistics**: Collect performance and system statistics

### Error Handling
- **Error History Tracking**: Maintain circular buffer of recent errors
- **Error History Size**: Number of errors to keep (5-50 entries)

### Debug Logging
- **Enable Logging**: Enable the debug logging system
- **Log Level**: Default verbosity (None/Error/Warning/Info/Debug)
- **Message Length**: Maximum log message size (32-512 characters)
- **Log Subsystems**: Enable logging for specific RTOS components
  - Core scheduler functions
  - Task management
  - Mutex operations
  - Queue operations
  - Timer operations
  - Memory management
  - Semaphore operations

### Advanced Configuration
- **Custom Tick Rate**: Override predefined tick rates
- **Runtime Assertions**: Enable assertion checks
- **Performance Profiling**: Enable profiling hooks

### Hardware Configuration
- **Hardware Timers**: Use hardware timers for system tick
- **Timer Interrupt Priority**: Priority level for system timer (0-7)
- **Context Switch Profiling**: Measure context switch performance

## Navigation

### Text Interface (menuconfig)

- **Arrow Keys**: Navigate menu items
- **Enter**: Select/enter submenu
- **Space**: Toggle boolean options
- **Escape**: Go back/exit
- **?**: Show help for current item
- **S**: Save configuration
- **Q**: Quit

### GUI Interface (guiconfig)

- **Mouse**: Click to navigate and select
- **Checkboxes**: Toggle boolean options
- **Dropdowns**: Select from multiple choices
- **Text Fields**: Enter numeric values
- **Save Button**: Save configuration
- **Quit Button**: Exit interface

## Configuration Files

### Input Files

- **Kconfig**: Main configuration definition file
- **.config**: Current configuration values (generated)
- **defconfig**: Default configuration (optional)

### Generated Files

- **cmake_config.cmake**: CMake configuration variables
- **include/pico_rtos_config.h**: C preprocessor definitions

## Example Workflows

### Development Configuration

```bash
# Start with defaults
make defconfig

# Customize for development
make menuconfig
# Enable: Debug, Logging (Debug level), All log subsystems
# Set: Higher tick rate (2000 Hz), More tasks (32)

# Build with development config
make build
```

### Production Configuration

```bash
# Start with defaults
make defconfig

# Customize for production
make menuconfig
# Disable: Debug, Logging
# Enable: All safety features (stack checking, memory tracking)
# Set: Standard tick rate (1000 Hz), Minimal resources

# Build optimized for production
make build
```

### Testing Configuration

```bash
# Load defaults
make defconfig

# Configure for testing
make menuconfig
# Enable: Tests, Debug, Error history, Runtime stats
# Set: Maximum resources for comprehensive testing

# Build tests
make tests
```

## Integration with Build System

### CMake Integration

The menuconfig system generates `cmake_config.cmake` which can be used with CMake:

```bash
# Configure build with menuconfig settings
cmake -C cmake_config.cmake .

# Or use the Makefile
make build  # Automatically uses cmake_config.cmake
```

### C Code Integration

Generated header file `include/pico_rtos_config.h` contains all configuration:

```c
#include "pico_rtos_config.h"

#if PICO_RTOS_ENABLE_LOGGING
    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_CORE, "System initialized");
#endif

#if PICO_RTOS_ENABLE_STACK_CHECKING
    pico_rtos_check_stack_overflow();
#endif
```

## Advanced Usage

### Custom Configuration Files

```bash
# Use custom configuration file
python3 scripts/menuconfig.py --config-file custom.config

# Generate custom output files
python3 scripts/menuconfig.py \
    --config-file custom.config \
    --cmake-file custom_cmake.cmake \
    --header-file custom_config.h
```

### Batch Configuration

```bash
# Create configuration script
cat > config_script.sh << 'EOF'
#!/bin/bash
python3 scripts/menuconfig.py --load-defaults --show-config
# Modify .config file programmatically if needed
python3 scripts/menuconfig.py --show-config
EOF

chmod +x config_script.sh
./config_script.sh
```

### Configuration Validation

The system automatically validates configuration options:

- Tick rate must be between 10-10000 Hz
- Task count must be between 1-64
- Stack sizes must meet minimum requirements
- Dependencies between features are checked

Invalid configurations will show error messages with suggestions.

## Troubleshooting

### Common Issues

1. **kconfiglib not found**
   ```bash
   pip install kconfiglib
   ```

2. **GUI not working**
   ```bash
   # Install tkinter (usually included with Python)
   # On Ubuntu/Debian:
   sudo apt-get install python3-tk
   
   # On macOS with Homebrew:
   brew install python-tk
   ```

3. **Permission denied on scripts**
   ```bash
   chmod +x menuconfig.sh
   chmod +x scripts/menuconfig.py
   ```

4. **Configuration not taking effect**
   ```bash
   # Ensure files are generated
   make configure
   
   # Check generated files
   ls -la cmake_config.cmake include/pico_rtos_config.h
   
   # Clean and rebuild
   make clean build
   ```

### Debug Configuration Issues

```bash
# Show current configuration
make showconfig

# Debug configuration generation
make debug-config

# Verify generated CMake config
cat cmake_config.cmake

# Verify generated header
cat include/pico_rtos_config.h
```

## Best Practices

### 1. Use Version Control

```bash
# Track configuration files
git add .config defconfig
git commit -m "Add project configuration"

# Ignore generated files
echo "cmake_config.cmake" >> .gitignore
echo "include/pico_rtos_config.h" >> .gitignore
```

### 2. Document Configuration Choices

```bash
# Save configuration with description
make savedefconfig
echo "# Production configuration for Project X" > defconfig.txt
cat defconfig >> defconfig.txt
```

### 3. Test Different Configurations

```bash
# Save current config
cp .config config_backup

# Test different settings
make menuconfig
make build
# Test...

# Restore if needed
cp config_backup .config
make configure
```

### 4. Automate Configuration

```bash
# Create configuration profiles
mkdir configs
cp .config configs/development.config
cp .config configs/production.config

# Load specific profile
cp configs/production.config .config
make configure build
```

## Integration Examples

### CI/CD Pipeline

```yaml
# .github/workflows/build.yml
- name: Configure RTOS
  run: |
    pip install kconfiglib
    cp configs/ci.config .config
    make configure

- name: Build
  run: make build
```

### Docker Build

```dockerfile
# Install dependencies
RUN pip install kconfiglib

# Copy configuration
COPY .config /app/.config

# Configure and build
RUN make configure build
```

The menuconfig system provides a powerful and flexible way to configure Pico-RTOS for different use cases while maintaining compatibility with existing build systems and workflows.