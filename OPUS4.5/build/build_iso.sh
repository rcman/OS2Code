#!/bin/bash

# OS/2 Clone ISO Build Script

set -e

echo "=========================================="
echo " OS/2 Clone ISO Build System"
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
OS_ISO="$BUILD_DIR/os2clone.iso"

echo -e "${YELLOW}[1/6] Assembling bootloader...${NC}"
nasm -f bin "$BOOT_DIR/boot.asm" -o "$BOOTLOADER"
echo -e "${GREEN}      Done! ($(stat -c %s "$BOOTLOADER") bytes)${NC}"

echo -e "${YELLOW}[2/6] Assembling kernel entry...${NC}"
nasm -f elf32 "$KERNEL_DIR/entry.asm" -o "$BUILD_DIR/entry.o"
echo -e "${GREEN}      Done!${NC}"

echo -e "${YELLOW}[3/6] Compiling C++ sources...${NC}"

# Compiler flags for freestanding 32-bit kernel
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

echo -e "${YELLOW}[4/6] Linking kernel...${NC}"
ld -m elf_i386 -T "$BUILD_DIR/linker.ld" \
    "$BUILD_DIR/entry.o" \
    "$BUILD_DIR/kernel.o" \
    "$BUILD_DIR/string.o" \
    "$BUILD_DIR/vga.o" \
    "$BUILD_DIR/keyboard.o" \
    "$BUILD_DIR/shell.o" \
    --oformat binary -o "$KERNEL_BIN"
echo -e "${GREEN}      Done! ($(stat -c %s "$KERNEL_BIN") bytes)${NC}"

echo -e "${YELLOW}[5/6] Creating ISO directory structure...${NC}"
mkdir -p "$BUILD_DIR/iso"

# Create a combined boot image: bootloader + kernel
# The bootloader is 2048 bytes (1 CD sector)
# The kernel follows immediately after
cat "$BOOTLOADER" "$KERNEL_BIN" > "$BUILD_DIR/iso/boot.img"

echo -e "${GREEN}      Done!${NC}"

echo -e "${YELLOW}[6/6] Creating bootable ISO image...${NC}"

# Use xorriso to create the ISO with El Torito boot support
# Note: We do NOT use -boot-info-table as it overwrites bytes 8-63 of the bootloader
xorriso -as mkisofs \
    -R -J \
    -V "OS2CLONE" \
    -b boot.img \
    -no-emul-boot \
    -boot-load-size 64 \
    -o "$OS_ISO" \
    "$BUILD_DIR/iso" 2>/dev/null

echo -e "${GREEN}      Done!${NC}"

# Clean up
rm -rf "$BUILD_DIR/iso"

echo ""
echo -e "${GREEN}=========================================="
echo " Build completed successfully!"
echo "==========================================${NC}"
echo ""
echo " Output: $OS_ISO"
echo " Size:   $(stat -c %s "$OS_ISO") bytes"
echo ""
echo " To run with QEMU:"
echo "   qemu-system-i386 -cdrom $OS_ISO -boot d -m 16M"
echo ""
