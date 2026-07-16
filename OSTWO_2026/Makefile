# Makefile for OS/Two
# OS/2-Compatible Operating System
# Uses native gcc with -m32 for 32-bit compilation

# Tools
CC = gcc
AS = nasm
LD = ld

# Output
TARGET = kernel.bin
ISO = ostwo.iso

# Source files
ASM_SRCS = boot.asm io.asm isr.asm switch.asm lx_thunks.asm
C_SRCS = kernel.c vga.c printf.c gdt.c idt.c timer.c keyboard.c mouse.c events.c syscall.c graphics.c ramfs.c pmm.c vmm.c usermode.c process.c scheduler.c test_proc.c exec.c speaker.c rtc.c semaphore.c thread.c vfs.c ramfs_vfs.c dos_file.c elf.c linux_syscall.c serial.c parallel.c ide.c vga_gfx.c ipc.c pci.c virtio.c net.c virtio_net.c gui.c vbe.c bga.c lx.c

# Object files
ASM_OBJS = $(ASM_SRCS:.asm=.o)
C_OBJS = $(C_SRCS:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# Compiler flags
CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pic -nostdlib
CFLAGS += -Wall -Wextra -O2
CFLAGS += -fno-builtin -fno-exceptions

# Linker flags
# --no-warn-rwx-segments: a flat single-segment kernel is intentional
# here (the kernel manages its own page permissions).
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib --no-warn-rwx-segments

# libgcc provides 64-bit division helpers (__udivdi3 etc.) needed by
# printf %llu and any other 64-bit arithmetic in freestanding 32-bit code
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)

# Assembler flags
ASFLAGS = -f elf32

# Default target
all: $(TARGET)

# Link kernel
$(TARGET): $(OBJS) linker.ld
	@echo "Linking $(TARGET)..."
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGCC)
	@echo "Done! Kernel size: $$(stat -c %s $(TARGET)) bytes"

# Compile C files
%.o: %.c types.h events.h vga.h ramfs.h pmm.h vmm.h usermode.h process.h scheduler.h dosapi.h speaker.h rtc.h vfs.h dos_file.h elf.h linux_syscall.h serial.h parallel.h ide.h vga_gfx.h ipc.h pci.h virtio.h net.h virtio_net.h gui.h vbe.h bga.h lx.h
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM files
%.o: %.asm
	@echo "Assembling $<..."
	$(AS) $(ASFLAGS) $< -o $@

# ============================================================
# 64-bit kernel (Phase B.0): 32-bit multiboot loader + long-mode
# trampoline embedding a native x86-64 payload
# ============================================================
CFLAGS64 = -m64 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone
CFLAGS64 += -mgeneral-regs-only -mcmodel=small -nostdlib
CFLAGS64 += -Wall -Wextra -O2 -fno-builtin -fno-exceptions

kmain64.o64: kmain64.c font8x8.h
	@echo "Compiling kmain64.c (x86-64)..."
	$(CC) $(CFLAGS64) -c kmain64.c -o kmain64.o64

entry64.o64: entry64.asm
	$(AS) -f elf64 entry64.asm -o entry64.o64

isr64.o64: isr64.asm
	$(AS) -f elf64 isr64.asm -o isr64.o64

pmm64.o64: pmm64.c
	$(CC) $(CFLAGS64) -c pmm64.c -o pmm64.o64

vmm64.o64: vmm64.c
	$(CC) $(CFLAGS64) -c vmm64.c -o vmm64.o64

elf64.o64: elf64.c
	$(CC) $(CFLAGS64) -c elf64.c -o elf64.o64

thunk32.h: thunk32.asm
	nasm -f bin thunk32.asm -o thunk32.bin
	python3 -c "b=open('thunk32.bin','rb').read().ljust(160,b'\0'); f=open('thunk32.h','w'); f.write('// Generated from thunk32.asm\nstatic const unsigned char thunk32_blob[] = {\n'); [f.write('    '+', '.join('0x%02x'%c for c in b[i:i+12])+',\n') for i in range(0,len(b),12)]; f.write('};\nstatic const unsigned int thunk32_blob_len = %d;\n'%len(b)); f.close()"

lx64.o64: lx64.c lx.h thunk32.h
	$(CC) $(CFLAGS64) -c lx64.c -o lx64.o64

sched64.o64: sched64.c
	$(CC) $(CFLAGS64) -c sched64.c -o sched64.o64

kbd64.o64: kbd64.c
	$(CC) $(CFLAGS64) -c kbd64.c -o kbd64.o64

mouse64.o64: mouse64.c
	$(CC) $(CFLAGS64) -c mouse64.c -o mouse64.o64

tasks64.o64: tasks64.asm
	$(AS) -f elf64 tasks64.asm -o tasks64.o64

OBJS64 = entry64.o64 kmain64.o64 isr64.o64 pmm64.o64 vmm64.o64 elf64.o64 \
         lx64.o64 sched64.o64 tasks64.o64 kbd64.o64 mouse64.o64

kernel64.elf: $(OBJS64) linker64.ld
	$(LD) -m elf_x86_64 -T linker64.ld -nostdlib -o kernel64.elf $(OBJS64)

kernel64.bin: kernel64.elf
	objcopy -O binary kernel64.elf kernel64.bin

boot64.o: boot64.asm kernel64.bin
	$(AS) -f elf32 boot64.asm -o boot64.o

ostwo64.bin: boot64.o linker64boot.ld
	$(LD) -m elf_i386 -T linker64boot.ld -nostdlib -o ostwo64.bin boot64.o
	@echo "Done! 64-bit image: $$(stat -c %s ostwo64.bin) bytes"

kernel64: ostwo64.bin

# Native 64-bit user application (loaded as a multiboot module)
userprogs/app64.elf: userprogs/app64.c userprogs/app64.ld
	$(CC) -m64 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
		-mcmodel=large -nostdlib -static -O2 -Wall -Wextra \
		-T userprogs/app64.ld -Wl,--build-id=none \
		-o userprogs/app64.elf userprogs/app64.c
	@echo "app64.elf: $$(stat -c %s userprogs/app64.elf) bytes"

app64: userprogs/app64.elf

# Run the 64-bit kernel with a native ELF64 app AND a genuine OS/2 LX
# executable running in 32-bit compatibility mode (the capstone: an
# OS/2 app under the 64-bit kernel). Swap hello_os2.exe for hello.bin
# to demo the flat-binary compat path instead.
run64: ostwo64.bin userprogs/app64.elf userprogs/hello_os2.exe
	@echo "Starting QEMU x86_64: 64-bit kernel + app64.elf + hello_os2.exe (OS/2 LX in compat mode)..."
	qemu-system-x86_64 -kernel ostwo64.bin \
		-initrd "userprogs/app64.elf,userprogs/hello_os2.exe" \
		-m 256M -vga std -serial stdio

userprogs/hello_os2.exe:
	cd userprogs && python3 mklx.py

# Run the 64-bit kernel's interactive shell (keyboard + framebuffer)
run64sh: ostwo64.bin
	@echo "Starting QEMU x86_64: 64-bit interactive shell..."
	qemu-system-x86_64 -kernel ostwo64.bin -m 256M -vga std -serial stdio

# Run the 64-bit preemptive multitasking demo directly (via cmdline)
run64mt: ostwo64.bin
	@echo "Starting QEMU x86_64: 64-bit preemptive scheduler demo..."
	qemu-system-x86_64 -kernel ostwo64.bin -append smp -m 256M -vga std -serial stdio

# Create bootable ISO (requires grub-mkrescue)
iso: $(TARGET)
	@echo "Creating bootable ISO..."
	mkdir -p isodir/boot/grub
	cp $(TARGET) isodir/boot/
	echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	echo 'set default=0' >> isodir/boot/grub/grub.cfg
	echo 'set gfxpayload=1024x768x32' >> isodir/boot/grub/grub.cfg
	echo 'menuentry "OS/Two" { multiboot /boot/$(TARGET) }' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir 2>/dev/null || grub2-mkrescue -o $(ISO) isodir 2>/dev/null || echo "ERROR: grub-mkrescue/grub2-mkrescue not available"
	rm -rf isodir

# Run in QEMU (direct kernel boot with VirtIO)
run: $(TARGET)
	@echo "Starting QEMU with VirtIO..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -serial stdio \
		-drive file=virtio-disk.img,if=none,id=drive0,format=raw \
		-device virtio-blk-pci,drive=drive0 \
		-netdev user,id=net0 -device virtio-net-pci,netdev=net0

# Run with high-resolution VBE display (1024x768x32)
run-hires: $(TARGET)
	@echo "Starting QEMU with VBE high-resolution display (1024x768x32)..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -vga std -serial stdio \
		-drive file=virtio-disk.img,if=none,id=drive0,format=raw \
		-device virtio-blk-pci,drive=drive0 \
		-netdev user,id=net0 -device virtio-net-pci,netdev=net0

# Run with VirtIO GPU (best graphics performance)
run-virtio-gpu: $(TARGET)
	@echo "Starting QEMU with VirtIO GPU..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -device virtio-vga \
		-display gtk,zoom-to-fit=on -serial stdio \
		-drive file=virtio-disk.img,if=none,id=drive0,format=raw \
		-device virtio-blk-pci,drive=drive0 \
		-netdev user,id=net0 -device virtio-net-pci,netdev=net0

# Run with curses display (for headless)
run-curses: $(TARGET)
	@echo "Starting QEMU (curses mode)..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -display curses

# Run without display (serial only)
run-nographic: $(TARGET)
	@echo "Starting QEMU (nographic mode)..."
	qemu-system-i386 -kernel $(TARGET) -m 64M -nographic

# Run from ISO with high-resolution display (recommended)
run-iso: $(ISO)
	@echo "Starting QEMU from ISO with VBE display (1024x768x32)..."
	qemu-system-i386 -cdrom $(ISO) -m 128M -vga std -serial stdio

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

.PHONY: all clean run run-hires run-virtio-gpu run-curses run-nographic run-iso debug iso info
