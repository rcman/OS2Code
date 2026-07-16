// File: pci.h
// PCI Bus Enumeration and Configuration

#ifndef PCI_H
#define PCI_H

#include "types.h"

// PCI Configuration Space Registers
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// PCI Configuration Space Offsets
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS_CODE      0x0B
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D

// PCI Command Register Bits
#define PCI_COMMAND_IO          0x0001  // Enable I/O Space
#define PCI_COMMAND_MEMORY      0x0002  // Enable Memory Space
#define PCI_COMMAND_MASTER      0x0004  // Enable Bus Mastering
#define PCI_COMMAND_INTERRUPT   0x0400  // Disable INTx interrupts

// PCI Header Types
#define PCI_HEADER_TYPE_NORMAL  0x00
#define PCI_HEADER_TYPE_BRIDGE  0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

// VirtIO Vendor ID
#define PCI_VENDOR_ID_REDHAT    0x1AF4

// VirtIO Device IDs (legacy)
#define PCI_DEVICE_ID_VIRTIO_NET    0x1000
#define PCI_DEVICE_ID_VIRTIO_BLOCK  0x1001
#define PCI_DEVICE_ID_VIRTIO_CONSOLE 0x1003
#define PCI_DEVICE_ID_VIRTIO_RNG    0x1005
#define PCI_DEVICE_ID_VIRTIO_BALLOON 0x1002
#define PCI_DEVICE_ID_VIRTIO_SCSI   0x1004

// VirtIO Device IDs (modern, transitional)
#define PCI_DEVICE_ID_VIRTIO_NET_MODERN     0x1041
#define PCI_DEVICE_ID_VIRTIO_BLOCK_MODERN   0x1042
#define PCI_DEVICE_ID_VIRTIO_CONSOLE_MODERN 0x1043
#define PCI_DEVICE_ID_VIRTIO_RNG_MODERN     0x1044
#define PCI_DEVICE_ID_VIRTIO_BALLOON_MODERN 0x1045
#define PCI_DEVICE_ID_VIRTIO_SCSI_MODERN    0x1048
#define PCI_DEVICE_ID_VIRTIO_GPU_MODERN     0x1050

// PCI Device Structure
typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision_id;
    uint8_t  header_type;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint32_t bar[6];
} pci_device_t;

// Maximum PCI devices we can track
#define MAX_PCI_DEVICES 32

// Initialize PCI subsystem
void pci_init(void);

// Read/Write PCI configuration space
uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

// Scan PCI bus for devices
void pci_scan_bus(void);

// Find devices by vendor/device ID
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);

// Get device by index
pci_device_t* pci_get_device(uint32_t index);

// Get number of detected devices
uint32_t pci_get_device_count(void);

// Enable bus mastering for a device
void pci_enable_bus_mastering(pci_device_t* dev);

// Get BAR address (physical address from BAR register)
uint32_t pci_get_bar_address(pci_device_t* dev, uint8_t bar_num);

// Check if BAR is I/O space
int pci_bar_is_io(uint32_t bar);

// Check if BAR is memory space
int pci_bar_is_memory(uint32_t bar);

#endif // PCI_H
