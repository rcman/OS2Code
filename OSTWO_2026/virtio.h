// File: virtio.h
// VirtIO Virtualization Standard Support

#ifndef VIRTIO_H
#define VIRTIO_H

#include "types.h"
#include "pci.h"

// VirtIO Device Status Bits
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

// VirtIO Feature Bits (common)
#define VIRTIO_F_NOTIFY_ON_EMPTY    (1 << 24)
#define VIRTIO_F_ANY_LAYOUT         (1 << 27)
#define VIRTIO_F_RING_INDIRECT_DESC (1 << 28)
#define VIRTIO_F_RING_EVENT_IDX     (1 << 29)

// VirtIO Block Device Feature Bits
#define VIRTIO_BLK_F_SIZE_MAX       (1 << 1)
#define VIRTIO_BLK_F_SEG_MAX        (1 << 2)
#define VIRTIO_BLK_F_GEOMETRY       (1 << 4)
#define VIRTIO_BLK_F_RO             (1 << 5)
#define VIRTIO_BLK_F_BLK_SIZE       (1 << 6)
#define VIRTIO_BLK_F_FLUSH          (1 << 9)
#define VIRTIO_BLK_F_TOPOLOGY       (1 << 10)

// VirtIO Block Request Types
#define VIRTIO_BLK_T_IN             0
#define VIRTIO_BLK_T_OUT            1
#define VIRTIO_BLK_T_FLUSH          4

// VirtIO Block Request Status
#define VIRTIO_BLK_S_OK             0
#define VIRTIO_BLK_S_IOERR          1
#define VIRTIO_BLK_S_UNSUPP         2

// Virtqueue Descriptor Flags
#define VIRTQ_DESC_F_NEXT           1
#define VIRTQ_DESC_F_WRITE          2
#define VIRTQ_DESC_F_INDIRECT       4

// Virtqueue Available Ring Flags
#define VIRTQ_AVAIL_F_NO_INTERRUPT  1

// Virtqueue Used Ring Flags
#define VIRTQ_USED_F_NO_NOTIFY      1

// Virtqueue Descriptor
typedef struct {
    uint64_t addr;              // Physical address
    uint32_t len;               // Length
    uint16_t flags;             // Flags
    uint16_t next;              // Next descriptor index (if NEXT flag set)
} __attribute__((packed)) virtq_desc_t;

// Virtqueue Available Ring
typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[256];         // Actually variable size, but we use 256
    uint16_t used_event;        // Only if VIRTIO_F_RING_EVENT_IDX
} __attribute__((packed)) virtq_avail_t;

// Virtqueue Used Element
typedef struct {
    uint32_t id;                // Descriptor chain head
    uint32_t len;               // Bytes written
} __attribute__((packed)) virtq_used_elem_t;

// Virtqueue Used Ring
typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[256]; // Actually variable size, but we use 256
    uint16_t avail_event;        // Only if VIRTIO_F_RING_EVENT_IDX
} __attribute__((packed)) virtq_used_t;

// Virtqueue Structure
typedef struct {
    uint32_t queue_size;
    uint32_t queue_index;

    // Descriptor table
    virtq_desc_t* desc;

    // Available ring
    virtq_avail_t* avail;

    // Used ring
    virtq_used_t* used;

    // Physical addresses
    uint32_t desc_phys;
    uint32_t avail_phys;
    uint32_t used_phys;

    // Queue management
    uint16_t last_used_idx;
    uint16_t free_desc_head;
} virtqueue_t;

// VirtIO Block Request
typedef struct {
    uint32_t type;              // VIRTIO_BLK_T_*
    uint32_t reserved;
    uint64_t sector;            // Sector number
} __attribute__((packed)) virtio_blk_req_header_t;

// VirtIO Device Structure
typedef struct {
    pci_device_t* pci_dev;
    uint32_t io_base;           // I/O base address (from BAR0)
    uint32_t features;
    uint8_t status;

    // Virtqueues
    virtqueue_t* queues[8];     // Up to 8 queues
    uint32_t num_queues;

    // Device-specific
    union {
        struct {
            uint64_t capacity;  // Capacity in 512-byte sectors
            uint32_t size_max;
            uint32_t seg_max;
            uint32_t blk_size;
        } blk;
    } config;
} virtio_device_t;

// VirtIO Legacy I/O Registers (relative to BAR0)
#define VIRTIO_PCI_HOST_FEATURES    0   // Host features (32-bit)
#define VIRTIO_PCI_GUEST_FEATURES   4   // Guest features (32-bit)
#define VIRTIO_PCI_QUEUE_PFN        8   // Queue page frame number (32-bit)
#define VIRTIO_PCI_QUEUE_SIZE       12  // Queue size (16-bit)
#define VIRTIO_PCI_QUEUE_SEL        14  // Queue selector (16-bit)
#define VIRTIO_PCI_QUEUE_NOTIFY     16  // Queue notify (16-bit)
#define VIRTIO_PCI_STATUS           18  // Device status (8-bit)
#define VIRTIO_PCI_ISR              19  // ISR status (8-bit)
#define VIRTIO_PCI_CONFIG           20  // Device-specific config

// Initialize VirtIO subsystem
void virtio_init(void);

// Probe for VirtIO devices
void virtio_probe_devices(void);

// Initialize a VirtIO device
int virtio_device_init(virtio_device_t* dev, pci_device_t* pci_dev);

// Virtqueue operations
virtqueue_t* virtq_create(virtio_device_t* dev, uint32_t queue_index);
void virtq_destroy(virtqueue_t* vq);
int virtq_add_buffer(virtqueue_t* vq, uint32_t* desc_indices, uint32_t count);
void virtq_kick(virtio_device_t* dev, uint32_t queue_index);
int virtq_get_buffer(virtqueue_t* vq, uint32_t* len);

// VirtIO Block Device
int virtio_blk_init(virtio_device_t* dev);
int virtio_blk_read(virtio_device_t* dev, uint64_t sector, uint32_t count, void* buffer);
int virtio_blk_write(virtio_device_t* dev, uint64_t sector, uint32_t count, const void* buffer);

// VirtIO I/O helpers (exposed for drivers)
uint8_t virtio_read8(virtio_device_t* dev, uint32_t offset);
void virtio_write8(virtio_device_t* dev, uint32_t offset, uint8_t value);
uint16_t virtio_read16(virtio_device_t* dev, uint32_t offset);
void virtio_write16(virtio_device_t* dev, uint32_t offset, uint16_t value);
uint32_t virtio_read32(virtio_device_t* dev, uint32_t offset);
void virtio_write32(virtio_device_t* dev, uint32_t offset, uint32_t value);

#endif // VIRTIO_H
