# Flashing and Testing Guide

This guide explains how to flash your Pico-RTOS applications to the Raspberry Pi Pico and monitor their output via serial communication.

## Prerequisites

- Raspberry Pi Pico board
- USB cable (data transfer capable)
- Built Pico-RTOS project (see [Getting Started](getting_started.md))

## Hardware Setup

1. **Connect your Raspberry Pi Pico to your computer via USB**
2. **The Pico will appear as a serial device** when running firmware
3. **For flashing, the Pico needs to be in BOOTSEL mode**

## Flashing Process

### Step 1: Enter BOOTSEL Mode

1. **Hold the BOOTSEL button** on the Pico
2. **Connect the USB cable** (or press reset while holding BOOTSEL if already connected)
3. **Release the BOOTSEL button**
4. **The Pico will appear as a USB drive** called "RPI-RP2"

### Step 2: Flash the Firmware

```bash
# Navigate to your build directory
cd build

# Copy the .uf2 file to the Pico
cp led_blinking.uf2 /Volumes/RPI-RP2/

# For tests, use:
cp mutex_test.uf2 /Volumes/RPI-RP2/
cp queue_test.uf2 /Volumes/RPI-RP2/
cp semaphore_test.uf2 /Volumes/RPI-RP2/
cp task_test.uf2 /Volumes/RPI-RP2/
cp timer_test.uf2 /Volumes/RPI-RP2/

# For other examples:
cp task_synchronization.uf2 /Volumes/RPI-RP2/
cp system_test.uf2 /Volumes/RPI-RP2/
```

**Note**: The Pico will automatically reboot and start running the new firmware after copying the .uf2 file.

## Monitoring Serial Output

### Step 1: Find the Serial Port

After flashing, the Pico will appear as a serial device:

```bash
# List USB serial devices (macOS)
ls /dev/tty.usbmodem*
# or
ls /dev/cu.usbmodem*

# On Linux, it might be:
ls /dev/ttyACM*
```

### Step 2: Connect to Serial Output

You have several options for connecting to the serial output:

#### Option A: Using `screen` (built-in on macOS/Linux)
```bash
# Replace with your actual device path
screen /dev/tty.usbmodem14101 115200

# To exit: Press Ctrl+A, then K, then Y
```

#### Option B: Using `minicom`
```bash
# Install minicom if not available
brew install minicom  # macOS
# or
sudo apt-get install minicom  # Linux

# Connect to the device
minicom -D /dev/tty.usbmodem14101 -b 115200

# To exit: Press Ctrl+A, then X
```

#### Option C: Using `cu` (built-in)
```bash
cu -l /dev/tty.usbmodem14101 -s 115200

# To exit: Type ~. (tilde followed by period)
```

### Step 3: Using the Helper Script

We've provided a helper script to automate the connection:

```bash
# Make the script executable
chmod +x connect_pico.sh

# Run the script
./connect_pico.sh
```

The script will automatically find your Pico and connect to it.

## Expected Output Examples

### LED Blinking Example
```
Starting Pico-RTOS LED Blinking Example...
Starting scheduler...
LED OFF
System uptime: 5000 ms, Status report #1
LED ON
Background task: completed 1000 work units
LED OFF
System uptime: 10000 ms, Status report #2
LED ON
```

### Task Synchronization Example
```
Task 1: Semaphore taken
Task 1: Semaphore given
Task 2: Semaphore taken
Task 2: Semaphore given
Task 1: Semaphore taken
Task 1: Semaphore given
```

### System Test Example
```
Starting Pico-RTOS System Test...
Creating tasks...
All tasks created successfully!
Starting scheduler...
High priority task started
Medium priority task started
Low priority task: Background work counter = 500

=== SYSTEM STATISTICS ===
Total tasks: 7
Ready tasks: 3
Blocked tasks: 2
Current memory: 2048 bytes
Peak memory: 2560 bytes
System uptime: 10000 ms
========================
```

### Test Output Examples

#### Mutex Test
```
Task 1: Shared resource value: 1
Task 2: Shared resource value: 2
Task 1: Shared resource value: 3
Task 2: Shared resource value: 4
```

#### Queue Test
```
Task 1: Sending item 1 to the queue
Task 2: Received item 1 from the queue
Task 1: Sending item 2 to the queue
Task 2: Received item 2 from the queue
```

#### Semaphore Test
```
Task 2: Giving semaphore
Task 1: Semaphore taken
Task 2: Giving semaphore
Task 1: Semaphore taken
```

#### Task Test
```
Task 1: Running
Task 2: Running
Task 1: Running
Task 2: Running
```

#### Timer Test
```
Timer callback: Timeout!
Timer callback: Timeout!
Timer callback: Timeout!
```

## Troubleshooting

### No Output Appears
1. **Check baud rate** - Ensure it's set to 115200
2. **Try different terminal programs** - Some work better than others
3. **Reset the Pico** - Press the reset button while connected
4. **Check USB cable** - Ensure it supports data transfer, not just power
5. **Verify the firmware flashed correctly** - Try reflashing

### Device Path Changes
```bash
# Monitor for new devices
ls /dev/tty.* | grep usbmodem

# Use system_profiler to see USB devices (macOS)
system_profiler SPUSBDataType | grep -A 10 -B 10 "Pico"

# Use lsusb on Linux
lsusb | grep -i pico
```

### Permission Issues (Linux)
```bash
# Add your user to the dialout group
sudo usermod -a -G dialout $USER

# Or use sudo for temporary access
sudo screen /dev/ttyACM0 115200
```

### Pico Not Recognized
1. **Try a different USB cable**
2. **Try a different USB port**
3. **Check if the Pico is properly seated**
4. **Verify the Pico is not damaged**

## Testing Workflow

### For Development Testing
1. **Build your project**: `make`
2. **Flash the firmware**: Copy .uf2 to Pico
3. **Connect to serial**: Use one of the methods above
4. **Observe output**: Verify expected behavior
5. **Make changes and repeat**

### For Automated Testing
You can create scripts to automate the testing process:

```bash
#!/bin/bash
# test_all.sh

TESTS=("mutex_test" "queue_test" "semaphore_test" "task_test" "timer_test")

for test in "${TESTS[@]}"; do
    echo "Testing $test..."
    cp "$test.uf2" /Volumes/RPI-RP2/
    sleep 5  # Wait for reboot
    # Add logic to capture and verify output
done
```

## Serial Communication Settings

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None

These settings are configured in the Pico SDK and should work with all examples and tests.

## Next Steps

- Try running different examples to see various RTOS features
- Modify the examples to experiment with different configurations
- Create your own applications using the RTOS API
- Refer to the [API Reference](api_reference.md) for detailed function documentation