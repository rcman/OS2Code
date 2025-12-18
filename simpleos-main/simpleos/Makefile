# Makefile for SimpleOS
# Uses native gcc with -m32 for 32-bit compilation

# Tools
CC = gcc
AS = nasm
LD = ld

# Output
TARGET = kernel.bin
ISO = simpleos.iso

# Source files
ASM_SRCS = boot.asm io.asm isr.asm switch.asm
C_SRCS = kernel.c vga.c printf.c gdt.c idt.c timer.c keyboard.c mouse.c events.c syscall.c graphics.c ramfs.c pmm.c vmm.c usermode.c process.c scheduler.c test_proc.c dosapi.c

# Object files
ASM_OBJS = $(ASM_SRCS:.asm=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# Compiler flags
CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pic -nostdlib
CFLAGS += -Wall -Wextra -O2
CFLAGS += -fno-builtin -fno-exceptions

# Linker flags
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Assembler flags
ASFLAGS = -f elf32

# Default target
all: $(TARGET)

# Link kernel
$(TARGET): $(OBJS) linker.ld
	@echo "Linking $(TARGET)..."
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "Done! Kernel size: $$(stat -c %s $(TARGET)) bytes"

# Compile C files
%.o: %.c types.h events.h vga.h ramfs.h pmm.h vmm.h usermode.h process.h scheduler.h dosapi.h
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM files
%.o: %.asm
	@echo "Assembling $<..."
	$(AS) $(ASFLAGS) $< -o $@

# Create bootable ISO (requires grub-mkrescue)
iso: $(TARGET)
	@echo "Creating bootable ISO..."
	mkdir -p isodir/boot/grub
	cp $(TARGET) isodir/boot/
	echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	echo 'set default=0' >> isodir/boot/grub/grub.cfg
	echo 'menuentry "SimpleOS" { multiboot /boot/$(TARGET) }' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir 2>/dev/null || grub2-mkrescue -o $(ISO) isodir 2>/dev/null || echo "ERROR: grub-mkrescue/grub2-mkrescue not available"
	rm -rf isodir

# Run in QEMU (direct kernel boot)
run: $(TARGET)
	@echo "Starting QEMU..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -serial stdio

# Run with curses display (for headless)
run-curses: $(TARGET)
	@echo "Starting QEMU (curses mode)..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -display curses

# Run without display (serial only)
run-nographic: $(TARGET)
	@echo "Starting QEMU (nographic mode)..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -nographic

# Debug with GDB
debug: $(TARGET)
	@echo "Starting QEMU in debug mode..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -s -S &
	@echo "Connect GDB with: target remote localhost:1234"

# Clean
clean:
	rm -f $(OBJS) $(TARGET) $(ISO)
	rm -rf isodir

# Show info
info:
	@echo "Source files: $(ASM_SRCS) $(C_SRCS)"
	@echo "Object files: $(OBJS)"
	@echo "Target: $(TARGET)"

.PHONY: all clean run run-curses run-nographic debug iso info
