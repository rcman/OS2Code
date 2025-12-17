#!/bin/bash

# OS/2 Clone Build Script

set -e

echo "=========================================="
echo " OS/2 Clone Build System"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directories
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BOOT_DIR="$ROOT_DIR/boot"
KERNEL_DIR="$ROOT_DIR/kernel"
DRIVERS_DIR="$ROOT_DIR/drivers"
SHELL_DIR="$ROOT_DIR/shell"

# Output files
BOOTLOADER="$BUILD_DIR/boot.bin"
KERNEL_BIN="$BUILD_DIR/kernel.bin"
OS_IMAGE="$BUILD_DIR/os2clone.img"

echo -e "${YELLOW}[1/5] Assembling bootloader...${NC}"
nasm -f bin "$BOOT_DIR/boot.asm" -o "$BOOTLOADER"
echo -e "${GREEN}      Done!${NC}"

echo -e "${YELLOW}[2/5] Assembling kernel entry...${NC}"
nasm -f elf32 "$KERNEL_DIR/entry.asm" -o "$BUILD_DIR/entry.o"
echo -e "${GREEN}      Done!${NC}"

echo -e "${YELLOW}[3/5] Compiling C++ sources...${NC}"

# Compiler flags
CXXFLAGS="-m32 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -fno-pie -fno-stack-protector -mno-red-zone -Wall -Wextra -O2"

# Compile kernel sources
g++ $CXXFLAGS -c "$KERNEL_DIR/kernel.cpp" -o "$BUILD_DIR/kernel.o" -I"$ROOT_DIR"
g++ $CXXFLAGS -c "$KERNEL_DIR/string.cpp" -o "$BUILD_DIR/string.o" -I"$ROOT_DIR"

# Compile drivers
g++ $CXXFLAGS -c "$DRIVERS_DIR/vga.cpp" -o "$BUILD_DIR/vga.o" -I"$ROOT_DIR"
g++ $CXXFLAGS -c "$DRIVERS_DIR/keyboard.cpp" -o "$BUILD_DIR/keyboard.o" -I"$ROOT_DIR"

# Compile shell
g++ $CXXFLAGS -c "$SHELL_DIR/shell.cpp" -o "$BUILD_DIR/shell.o" -I"$ROOT_DIR"

echo -e "${GREEN}      Done!${NC}"

echo -e "${YELLOW}[4/5] Linking kernel...${NC}"
ld -m elf_i386 -T "$BUILD_DIR/linker.ld" -o "$BUILD_DIR/kernel.elf" \
    "$BUILD_DIR/entry.o" \
    "$BUILD_DIR/kernel.o" \
    "$BUILD_DIR/string.o" \
    "$BUILD_DIR/vga.o" \
    "$BUILD_DIR/keyboard.o" \
    "$BUILD_DIR/shell.o" \
    --oformat binary -o "$KERNEL_BIN"
echo -e "${GREEN}      Done!${NC}"

echo -e "${YELLOW}[5/5] Creating disk image...${NC}"

# Create a 1.44MB floppy disk image
dd if=/dev/zero of="$OS_IMAGE" bs=512 count=2880 2>/dev/null

# Write bootloader to first sector
dd if="$BOOTLOADER" of="$OS_IMAGE" bs=512 count=1 conv=notrunc 2>/dev/null

# Write kernel starting at sector 2
dd if="$KERNEL_BIN" of="$OS_IMAGE" bs=512 seek=1 conv=notrunc 2>/dev/null

echo -e "${GREEN}      Done!${NC}"

echo ""
echo -e "${GREEN}=========================================="
echo " Build completed successfully!"
echo "==========================================${NC}"
echo ""
echo " Output: $OS_IMAGE"
echo ""
echo " To run with QEMU:"
echo "   qemu-system-i386 -drive file=$OS_IMAGE,format=raw,if=floppy -boot a -m 16M"
echo ""
