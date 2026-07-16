// File: pci.c
// PCI Bus Enumeration and Configuration Implementation

#include "pci.h"

// External functions
extern void printf(const char* format, ...);
extern void outl(uint16_t port, uint32_t value);
extern uint32_t inl(uint16_t port);
extern void outw(uint16_t port, uint16_t value);
extern uint16_t inw(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);

// PCI device table
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

// Build PCI configuration address
static uint32_t pci_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (uint32_t)(
        (1 << 31) |                     // Enable bit
        ((uint32_t)bus << 16) |         // Bus number
        ((uint32_t)device << 11) |      // Device number
        ((uint32_t)function << 8) |     // Function number
        (offset & 0xFC)                 // Register offset (aligned to 4 bytes)
    );
}

// Read 32-bit value from PCI configuration space
uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

// Read 16-bit value from PCI configuration space
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

// Read 8-bit value from PCI configuration space
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return (uint8_t)((inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF);
}

// Write 32-bit value to PCI configuration space
void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

// Write 16-bit value to PCI configuration space
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t data = inl(PCI_CONFIG_DATA);
    data &= ~(0xFFFF << ((offset & 2) * 8));
    data |= (uint32_t)value << ((offset & 2) * 8);
    outl(PCI_CONFIG_DATA, data);
}

// Write 8-bit value to PCI configuration space
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t data = inl(PCI_CONFIG_DATA);
    data &= ~(0xFF << ((offset & 3) * 8));
    data |= (uint32_t)value << ((offset & 3) * 8);
    outl(PCI_CONFIG_DATA, data);
}

// Check if a PCI device exists
static int pci_device_exists(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_config_read_word(bus, device, function, PCI_VENDOR_ID);
    return (vendor_id != 0xFFFF);
}

// Add device to our table
static void pci_add_device(uint8_t bus, uint8_t device, uint8_t function) {
    if (pci_device_count >= MAX_PCI_DEVICES) {
        return;  // Table full
    }

    pci_device_t* dev = &pci_devices[pci_device_count];

    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    dev->vendor_id = pci_config_read_word(bus, device, function, PCI_VENDOR_ID);
    dev->device_id = pci_config_read_word(bus, device, function, PCI_DEVICE_ID);
    dev->class_code = pci_config_read_byte(bus, device, function, PCI_CLASS_CODE);
    dev->subclass = pci_config_read_byte(bus, device, function, PCI_SUBCLASS);
    dev->prog_if = pci_config_read_byte(bus, device, function, PCI_PROG_IF);
    dev->revision_id = pci_config_read_byte(bus, device, function, PCI_REVISION_ID);
    dev->header_type = pci_config_read_byte(bus, device, function, PCI_HEADER_TYPE);
    dev->interrupt_line = pci_config_read_byte(bus, device, function, PCI_INTERRUPT_LINE);
    dev->interrupt_pin = pci_config_read_byte(bus, device, function, PCI_INTERRUPT_PIN);

    // Read BARs
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_config_read_dword(bus, device, function, PCI_BAR0 + (i * 4));
    }

    pci_device_count++;

    printf("[PCI] %02x:%02x.%x - Vendor: 0x%04x Device: 0x%04x Class: 0x%02x/0x%02x\n",
           bus, device, function, dev->vendor_id, dev->device_id,
           dev->class_code, dev->subclass);
}

// Scan PCI bus for devices
void pci_scan_bus(void) {
    pci_device_count = 0;

    // Scan bus 0 (most systems only have bus 0)
    for (uint16_t bus = 0; bus < 1; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            if (pci_device_exists(bus, device, 0)) {
                pci_add_device(bus, device, 0);

                // Check for multi-function device
                uint8_t header_type = pci_config_read_byte(bus, device, 0, PCI_HEADER_TYPE);
                if (header_type & 0x80) {
                    // Multi-function device, check other functions
                    for (uint8_t function = 1; function < 8; function++) {
                        if (pci_device_exists(bus, device, function)) {
                            pci_add_device(bus, device, function);
                        }
                    }
                }
            }
        }
    }
}

// Find device by vendor/device ID
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Get device by index
pci_device_t* pci_get_device(uint32_t index) {
    if (index >= pci_device_count) {
        return NULL;
    }
    return &pci_devices[index];
}

// Get number of detected devices
uint32_t pci_get_device_count(void) {
    return pci_device_count;
}

// Enable bus mastering for a device
void pci_enable_bus_mastering(pci_device_t* dev) {
    uint16_t command = pci_config_read_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
    pci_config_write_word(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

// Get BAR address
uint32_t pci_get_bar_address(pci_device_t* dev, uint8_t bar_num) {
    if (bar_num >= 6) {
        return 0;
    }

    uint32_t bar = dev->bar[bar_num];

    if (pci_bar_is_io(bar)) {
        return bar & 0xFFFFFFFC;  // I/O space, mask lower 2 bits
    } else {
        return bar & 0xFFFFFFF0;  // Memory space, mask lower 4 bits
    }
}

// Check if BAR is I/O space
int pci_bar_is_io(uint32_t bar) {
    return (bar & 0x1) != 0;
}

// Check if BAR is memory space
int pci_bar_is_memory(uint32_t bar) {
    return (bar & 0x1) == 0;
}

// Initialize PCI subsystem
void pci_init(void) {
    printf("[PCI] Initializing PCI subsystem...\n");
    pci_scan_bus();
    printf("[PCI] Found %d PCI device(s)\n", pci_device_count);
}
