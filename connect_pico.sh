#!/bin/bash

# Find Pico serial device
DEVICE=$(ls /dev/tty.usbmodem* 2>/dev/null | head -1)

if [ -z "$DEVICE" ]; then
    echo "No Pico device found. Make sure it's connected and not in BOOTSEL mode."
    exit 1
fi

echo "Connecting to Pico at $DEVICE..."
echo "Press Ctrl+A then K then Y to exit screen"
sleep 2

# Connect using screen
screen $DEVICE 115200