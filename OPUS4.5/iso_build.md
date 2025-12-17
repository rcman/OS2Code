# Old (floppy):
qemu-system-i386 -drive file=os2clone.img,format=raw,if=floppy -boot a

# New (CD-ROM):
qemu-system-i386 -cdrom os2clone.iso -boot d -m 16M

Summary of Changes for ISO Boot
1. New Bootloader (boot_iso.asm)
The key change is that the bootloader no longer reads from disk. Instead:

The BIOS/El Torito loader loads the entire boot image (bootloader + kernel) into memory at 0x7C00
The bootloader copies the kernel from 0x8400 (where it was loaded) to 0x1000 (where the linker expects it)
Then proceeds to protected mode and jumps to kernel

2. Build Script Changes (build_iso.sh)

Creates a combined boot.img (bootloader + kernel concatenated)
Uses xorriso to create an El Torito bootable ISO
Important: Does NOT use -boot-info-table (it corrupts the bootloader)
Uses -boot-load-size 64 to load 32KB of the boot image
