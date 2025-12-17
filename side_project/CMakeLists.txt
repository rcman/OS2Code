# OSFree Modern Build System
# Replaces legacy Watcom makefiles with modern CMake

cmake_minimum_required(VERSION 3.20)

project(OSFree 
    VERSION 5.0.0
    DESCRIPTION "Modern OS/2 compatible operating system"
    LANGUAGES C ASM_NASM
)

# ============================================
# Build Configuration
# ============================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# ============================================
# Compiler Settings
# ============================================

# Kernel must be freestanding
set(KERNEL_C_FLAGS
    -ffreestanding
    -nostdlib
    -nostdinc
    -fno-builtin
    -fno-stack-protector
    -fno-pic
    -fno-pie
    -mno-red-zone          # Required for x86-64 kernel
    -mcmodel=kernel        # Kernel code model
    -mno-sse               # No SSE in kernel (saves state)
    -mno-mmx
    -mno-sse2
    -mno-3dnow
    -m64                   # 64-bit kernel
)

# Warning flags
set(KERNEL_WARNING_FLAGS
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
    -Wno-unused-function
    -Wstrict-prototypes
    -Wold-style-definition
)

# Optimization flags
set(KERNEL_OPT_FLAGS_DEBUG -O0 -g3)
set(KERNEL_OPT_FLAGS_RELEASE -O2 -flto)

# ============================================
# Architecture Detection
# ============================================

set(ARCH "x86_64" CACHE STRING "Target architecture")
set_property(CACHE ARCH PROPERTY STRINGS x86_64 i686)

if(ARCH STREQUAL "x86_64")
    add_compile_definitions(__x86_64__)
    set(ARCH_SPECIFIC_FLAGS -m64)
elseif(ARCH STREQUAL "i686")
    add_compile_definitions(__i686__)
    set(ARCH_SPECIFIC_FLAGS -m32)
else()
    message(FATAL_ERROR "Unsupported architecture: ${ARCH}")
endif()

# ============================================
# Source Organization
# ============================================

set(KERNEL_DIR ${CMAKE_SOURCE_DIR}/kernel)
set(DRIVERS_DIR ${CMAKE_SOURCE_DIR}/drivers)
set(BOOT_DIR ${CMAKE_SOURCE_DIR}/boot)
set(OS2_DIR ${CMAKE_SOURCE_DIR}/os2)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${KERNEL_DIR}/include
)

# ============================================
# Kernel Core Components
# ============================================

set(KERNEL_SOURCES
    # Main kernel
    ${KERNEL_DIR}/main.c
    ${KERNEL_DIR}/init.c
    
    # Scheduler
    ${KERNEL_DIR}/sched/sched_core.c
    ${KERNEL_DIR}/sched/sched_smp.c
    ${KERNEL_DIR}/sched/thread.c
    ${KERNEL_DIR}/sched/process.c
    
    # Hardware Abstraction Layer
    ${KERNEL_DIR}/hal/hal_init.c
    ${KERNEL_DIR}/hal/hal_cpu.c
    ${KERNEL_DIR}/hal/hal_apic.c
    ${KERNEL_DIR}/hal/hal_acpi.c
    ${KERNEL_DIR}/hal/hal_timer.c
    
    # Memory Management
    ${KERNEL_DIR}/mm/pmm.c          # Physical memory manager
    ${KERNEL_DIR}/mm/vmm.c          # Virtual memory manager
    ${KERNEL_DIR}/mm/heap.c         # Kernel heap
    ${KERNEL_DIR}/mm/slab.c         # Slab allocator
    
    # Interrupt Handling
    ${KERNEL_DIR}/irq/idt.c
    ${KERNEL_DIR}/irq/irq.c
    ${KERNEL_DIR}/irq/isr.c
    
    # Synchronization
    ${KERNEL_DIR}/sync/spinlock.c
    ${KERNEL_DIR}/sync/mutex.c
    ${KERNEL_DIR}/sync/semaphore.c
    ${KERNEL_DIR}/sync/rwlock.c
    
    # IPC (Inter-Process Communication)
    ${KERNEL_DIR}/ipc/pipe.c
    ${KERNEL_DIR}/ipc/queue.c
    ${KERNEL_DIR}/ipc/shm.c
    
    # Device Management
    ${KERNEL_DIR}/dev/device.c
    ${KERNEL_DIR}/dev/pci.c
    ${KERNEL_DIR}/dev/dma.c
    
    # File System Layer
    ${KERNEL_DIR}/fs/vfs.c
    ${KERNEL_DIR}/fs/file.c
    ${KERNEL_DIR}/fs/dcache.c
    
    # Utilities
    ${KERNEL_DIR}/lib/string.c
    ${KERNEL_DIR}/lib/printf.c
    ${KERNEL_DIR}/lib/atomic.c
)

set(KERNEL_ASM_SOURCES
    ${KERNEL_DIR}/arch/${ARCH}/entry.asm
    ${KERNEL_DIR}/arch/${ARCH}/switch.asm
    ${KERNEL_DIR}/arch/${ARCH}/interrupt.asm
    ${KERNEL_DIR}/arch/${ARCH}/ap_trampoline.asm
)

# ============================================
# OS/2 Personality Layer
# ============================================

set(OS2_SOURCES
    ${OS2_DIR}/dosapi/doscalls.c
    ${OS2_DIR}/dosapi/doserror.c
    ${OS2_DIR}/dosapi/dosfile.c
    ${OS2_DIR}/dosapi/dosmem.c
    ${OS2_DIR}/dosapi/dosthread.c
    ${OS2_DIR}/dosapi/dosprocess.c
    ${OS2_DIR}/dosapi/dossleep.c
    ${OS2_DIR}/dosapi/dossem.c
    ${OS2_DIR}/dosapi/dosqueue.c
    ${OS2_DIR}/dosapi/dospipe.c
    
    # 16-bit compatibility
    ${OS2_DIR}/thunk/thunk16.c
    ${OS2_DIR}/thunk/selector.c
    
    # Workplace Shell components
    ${OS2_DIR}/wps/wps_core.c
    ${OS2_DIR}/pm/pm_core.c
)

# ============================================
# File System Drivers
# ============================================

set(FS_SOURCES
    # JFS - Native OS/2 filesystem
    ${KERNEL_DIR}/fs/jfs/jfs_super.c
    ${KERNEL_DIR}/fs/jfs/jfs_inode.c
    ${KERNEL_DIR}/fs/jfs/jfs_dmap.c
    ${KERNEL_DIR}/fs/jfs/jfs_imap.c
    
    # FAT for compatibility
    ${KERNEL_DIR}/fs/fat/fat_super.c
    ${KERNEL_DIR}/fs/fat/fat_inode.c
    ${KERNEL_DIR}/fs/fat/fat_dir.c
    
    # ext4 for Linux interop
    ${KERNEL_DIR}/fs/ext4/ext4_super.c
    ${KERNEL_DIR}/fs/ext4/ext4_inode.c
)

# ============================================
# Device Drivers
# ============================================

set(DRIVER_SOURCES
    # Storage
    ${DRIVERS_DIR}/storage/nvme/nvme_core.c
    ${DRIVERS_DIR}/storage/nvme/nvme_pci.c
    ${DRIVERS_DIR}/storage/ahci/ahci.c
    
    # Network
    ${DRIVERS_DIR}/net/e1000e/e1000e_main.c
    ${DRIVERS_DIR}/net/rtl8169/rtl8169.c
    
    # USB
    ${DRIVERS_DIR}/usb/xhci/xhci_main.c
    ${DRIVERS_DIR}/usb/xhci/xhci_ring.c
    ${DRIVERS_DIR}/usb/core/usb_core.c
    
    # Graphics
    ${DRIVERS_DIR}/video/vesa/vesa.c
    ${DRIVERS_DIR}/video/framebuffer/fb.c
    
    # Input
    ${DRIVERS_DIR}/input/keyboard.c
    ${DRIVERS_DIR}/input/mouse.c
    ${DRIVERS_DIR}/input/ps2.c
)

# ============================================
# Build Targets
# ============================================

# Kernel executable
add_executable(osfree.elf
    ${KERNEL_SOURCES}
    ${KERNEL_ASM_SOURCES}
    ${OS2_SOURCES}
    ${FS_SOURCES}
)

target_compile_options(osfree.elf PRIVATE
    ${KERNEL_C_FLAGS}
    ${KERNEL_WARNING_FLAGS}
    ${ARCH_SPECIFIC_FLAGS}
    $<$<CONFIG:Debug>:${KERNEL_OPT_FLAGS_DEBUG}>
    $<$<CONFIG:Release>:${KERNEL_OPT_FLAGS_RELEASE}>
)

target_link_options(osfree.elf PRIVATE
    -nostdlib
    -static
    -Wl,-T,${CMAKE_SOURCE_DIR}/linker.ld
    -Wl,--build-id=none
    -Wl,-z,max-page-size=0x1000
)

# Drivers as loadable modules
foreach(driver ${DRIVER_SOURCES})
    get_filename_component(driver_name ${driver} NAME_WE)
    add_library(${driver_name} MODULE ${driver})
    
    target_compile_options(${driver_name} PRIVATE
        ${KERNEL_C_FLAGS}
        ${KERNEL_WARNING_FLAGS}
    )
    
    set_target_properties(${driver_name} PROPERTIES
        PREFIX ""
        SUFFIX ".ko"
    )
endforeach()

# ============================================
# Bootloader
# ============================================

add_executable(osfree_boot.efi
    ${BOOT_DIR}/efi/main.c
    ${BOOT_DIR}/efi/console.c
    ${BOOT_DIR}/efi/loader.c
    ${BOOT_DIR}/efi/memory.c
)

target_compile_options(osfree_boot.efi PRIVATE
    -fno-stack-protector
    -fpic
    -fshort-wchar
    -mno-red-zone
)

target_link_options(osfree_boot.efi PRIVATE
    -shared
    -Bsymbolic
    -nostdlib
    -Wl,-dll
    -Wl,--subsystem,10
    -e efi_main
)

# ============================================
# ISO Image Generation
# ============================================

find_program(GRUB_MKRESCUE grub-mkrescue)
find_program(XORRISO xorriso)

if(GRUB_MKRESCUE AND XORRISO)
    add_custom_target(iso
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:osfree.elf> ${CMAKE_BINARY_DIR}/iso/boot/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/grub.cfg ${CMAKE_BINARY_DIR}/iso/boot/grub/
        COMMAND ${GRUB_MKRESCUE} -o ${CMAKE_BINARY_DIR}/osfree.iso ${CMAKE_BINARY_DIR}/iso
        DEPENDS osfree.elf
        COMMENT "Creating bootable ISO image"
    )
else()
    message(WARNING "grub-mkrescue or xorriso not found. 'make iso' target will not be available.")
endif()

# ============================================
# Testing with QEMU
# ============================================

find_program(QEMU qemu-system-x86_64)

if(QEMU)
    add_custom_target(qemu
        COMMAND ${QEMU}
            -machine q35
            -cpu host
            -enable-kvm
            -smp 4
            -m 2G
            -serial stdio
            -kernel $<TARGET_FILE:osfree.elf>
            -append "console=ttyS0"
        DEPENDS osfree.elf
        COMMENT "Running OSFree in QEMU"
    )
    
    add_custom_target(qemu-debug
        COMMAND ${QEMU}
            -machine q35
            -cpu host
            -enable-kvm
            -smp 4
            -m 2G
            -serial stdio
            -kernel $<TARGET_FILE:osfree.elf>
            -append "console=ttyS0"
            -s -S
        DEPENDS osfree.elf
        COMMENT "Running OSFree in QEMU with GDB server"
    )
else()
    message(WARNING "QEMU not found. 'make qemu' target will not be available.")
endif()

# ============================================
# Documentation
# ============================================

find_program(DOXYGEN doxygen)

if(DOXYGEN)
    add_custom_target(docs
        COMMAND ${DOXYGEN} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating documentation with Doxygen"
    )
endif()

# ============================================
# Installation
# ============================================

install(TARGETS osfree.elf
    RUNTIME DESTINATION boot
)

install(FILES ${CMAKE_BINARY_DIR}/osfree.iso
    DESTINATION .
    OPTIONAL
)

# Install drivers
foreach(driver ${DRIVER_SOURCES})
    get_filename_component(driver_name ${driver} NAME_WE)
    install(TARGETS ${driver_name}
        LIBRARY DESTINATION lib/modules
    )
endforeach()

# ============================================
# Summary
# ============================================

message(STATUS "")
message(STATUS "OSFree Build Configuration")
message(STATUS "==========================")
message(STATUS "Version:           ${PROJECT_VERSION}")
message(STATUS "Architecture:      ${ARCH}")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler:        ${CMAKE_C_COMPILER}")
message(STATUS "Assembler:         ${CMAKE_ASM_NASM_COMPILER}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  osfree.elf       - Kernel binary")
message(STATUS "  osfree_boot.efi  - UEFI bootloader")
if(GRUB_MKRESCUE)
    message(STATUS "  iso              - Bootable ISO image")
endif()
if(QEMU)
    message(STATUS "  qemu             - Run in QEMU")
    message(STATUS "  qemu-debug       - Run in QEMU with GDB")
endif()
if(DOXYGEN)
    message(STATUS "  docs             - Generate documentation")
endif()
message(STATUS "")