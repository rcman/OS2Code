#!/bin/bash

# OS/2 Clone Run Script

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
OS_IMAGE="$BUILD_DIR/os2clone.img"

# Check if image exists
if [ ! -f "$OS_IMAGE" ]; then
    echo "Error: OS image not found. Run build.sh first."
    exit 1
fi

echo "Starting OS/2 Clone in QEMU..."
echo "Press Ctrl+Alt+G to release mouse, Ctrl+C in terminal to quit"
echo ""

# Run QEMU with appropriate settings
qemu-system-i386 \
    -drive file="$OS_IMAGE",format=raw,if=floppy \
    -boot a \
    -m 16M \
    -display curses \
    -no-reboot \
    -no-shutdown
