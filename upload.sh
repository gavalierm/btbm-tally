#!/bin/bash

# Path to ESP-IDF directory
# IDF_PATH="esp-idf"

PORT="/dev/cu.usbmodem2101"  # Change this to your board's USB port

# Set up ESP-IDF environment
# source "$IDF_PATH/export.sh"

# Build and upload the code
ls /dev/cu.*

#idf.py set-target esp32s3
#idf.py build
idf.py -p "$PORT" -b 115200 flash monitor
