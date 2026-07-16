// File: virtio.c
// VirtIO Virtualization Standard Implementation

#include "virtio.h"
#include "pmm.h"

// External functions
extern void printf(const char* format, ...);
extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);
extern void outw(uint16_t port, uint16_t value);
extern uint16_t inw(uint16_t port);
extern void outl(uint16_t port, uint32_t value);
extern uint32_t inl(uint16_t port);
extern uint32_t pmm_alloc_page(void);
extern void pmm_free_page(uint32_t addr);

// VirtIO device registry
static virtio_device_t* virtio_devices[8];
static uint32_t virtio_device_count = 0;

// Simple memory set
static void mem_set(void* dest, uint8_t val, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = val;
    }
}

// Read VirtIO register (8-bit)
uint8_t virtio_read8(virtio_device_t* dev, uint32_t offset) {
    return inb(dev->io_base + offset);
}

// Write VirtIO register (8-bit)
void virtio_write8(virtio_device_t* dev, uint32_t offset, uint8_t value) {
    outb(dev->io_base + offset, value);
}

// Read VirtIO register (16-bit)
uint16_t virtio_read16(virtio_device_t* dev, uint32_t offset) {
    return inw(dev->io_base + offset);
}

// Write VirtIO register (16-bit)
void virtio_write16(virtio_device_t* dev, uint32_t offset, uint16_t value) {
    outw(dev->io_base + offset, value);
}

// Read VirtIO register (32-bit)
uint32_t virtio_read32(virtio_device_t* dev, uint32_t offset) {
    return inl(dev->io_base + offset);
}

// Write VirtIO register (32-bit)
void virtio_write32(virtio_device_t* dev, uint32_t offset, uint32_t value) {
    outl(dev->io_base + offset, value);
}

// Create a virtqueue
virtqueue_t* virtq_create(virtio_device_t* dev, uint32_t queue_index) {
    // Select queue
    virtio_write16(dev, VIRTIO_PCI_QUEUE_SEL, queue_index);

    // Get queue size
    uint16_t queue_size = virtio_read16(dev, VIRTIO_PCI_QUEUE_SIZE);
    if (queue_size == 0) {
        printf("[VirtIO] Queue %d not available\n", queue_index);
        return NULL;
    }

    printf("[VirtIO] Creating queue %d (size=%d)\n", queue_index, queue_size);

    // Allocate virtqueue structure
    virtqueue_t* vq = (virtqueue_t*)pmm_alloc_page();
    if (!vq) {
        return NULL;
    }
    mem_set(vq, 0, 4096);

    vq->queue_size = queue_size;
    vq->queue_index = queue_index;
    vq->last_used_idx = 0;
    vq->free_desc_head = 0;

    // Calculate sizes
    uint32_t desc_size = 16 * queue_size;                  // 16 bytes per descriptor
    uint32_t avail_size = 6 + 2 * queue_size + 2;          // header + ring + used_event
    uint32_t used_size = 6 + 8 * queue_size + 2;           // header + ring + avail_event

    // Allocate memory for descriptor table (aligned to 16)
    uint32_t desc_phys = pmm_alloc_page();
    if (!desc_phys) {
        pmm_free_page((uint32_t)vq);
        return NULL;
    }
    vq->desc = (virtq_desc_t*)desc_phys;
    vq->desc_phys = desc_phys;
    mem_set(vq->desc, 0, desc_size);

    // Initialize free descriptor chain
    for (uint16_t i = 0; i < queue_size - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[queue_size - 1].next = 0xFFFF;  // End of chain

    // Allocate memory for available ring (aligned to 2)
    uint32_t avail_phys = pmm_alloc_page();
    if (!avail_phys) {
        pmm_free_page(desc_phys);
        pmm_free_page((uint32_t)vq);
        return NULL;
    }
    vq->avail = (virtq_avail_t*)avail_phys;
    vq->avail_phys = avail_phys;
    mem_set(vq->avail, 0, avail_size);

    // Allocate memory for used ring (aligned to 4)
    uint32_t used_phys = pmm_alloc_page();
    if (!used_phys) {
        pmm_free_page(avail_phys);
        pmm_free_page(desc_phys);
        pmm_free_page((uint32_t)vq);
        return NULL;
    }
    vq->used = (virtq_used_t*)used_phys;
    vq->used_phys = used_phys;
    mem_set(vq->used, 0, used_size);

    // Tell device about queue (legacy interface uses page frame number)
    uint32_t pfn = desc_phys >> 12;  // Page frame number
    virtio_write32(dev, VIRTIO_PCI_QUEUE_PFN, pfn);

    return vq;
}

// Destroy a virtqueue
void virtq_destroy(virtqueue_t* vq) {
    if (!vq) return;

    if (vq->desc_phys) pmm_free_page(vq->desc_phys);
    if (vq->avail_phys) pmm_free_page(vq->avail_phys);
    if (vq->used_phys) pmm_free_page(vq->used_phys);
    pmm_free_page((uint32_t)vq);
}

// Add buffers to virtqueue
int virtq_add_buffer(virtqueue_t* vq, uint32_t* desc_indices, uint32_t count) {
    if (count == 0) return -1;

    uint16_t head = desc_indices[0];

    // Chain descriptors
    for (uint32_t i = 0; i < count - 1; i++) {
        vq->desc[desc_indices[i]].flags = VIRTQ_DESC_F_NEXT;
        vq->desc[desc_indices[i]].next = desc_indices[i + 1];
    }
    vq->desc[desc_indices[count - 1]].flags = 0;  // Last descriptor

    // Add to available ring
    uint16_t avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->queue_size] = head;
    vq->avail->idx = avail_idx + 1;

    return 0;
}

// Kick virtqueue (notify device)
void virtq_kick(virtio_device_t* dev, uint32_t queue_index) {
    virtio_write16(dev, VIRTIO_PCI_QUEUE_NOTIFY, queue_index);
}

// Get completed buffer from virtqueue
int virtq_get_buffer(virtqueue_t* vq, uint32_t* len) {
    if (vq->last_used_idx == vq->used->idx) {
        return -1;  // No completed buffers
    }

    virtq_used_elem_t* elem = &vq->used->ring[vq->last_used_idx % vq->queue_size];
    if (len) *len = elem->len;

    vq->last_used_idx++;
    return elem->id;
}

// Initialize VirtIO device
int virtio_device_init(virtio_device_t* dev, pci_device_t* pci_dev) {
    dev->pci_dev = pci_dev;
    dev->num_queues = 0;

    // Get I/O base from BAR0
    uint32_t bar0 = pci_get_bar_address(pci_dev, 0);
    if (!pci_bar_is_io(pci_dev->bar[0])) {
        printf("[VirtIO] BAR0 is not I/O space\n");
        return -1;
    }
    dev->io_base = bar0;

    printf("[VirtIO] Device I/O base: 0x%x\n", dev->io_base);

    // Reset device
    virtio_write8(dev, VIRTIO_PCI_STATUS, 0);

    // Set ACKNOWLEDGE status
    uint8_t status = VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_write8(dev, VIRTIO_PCI_STATUS, status);

    // Set DRIVER status
    status |= VIRTIO_STATUS_DRIVER;
    virtio_write8(dev, VIRTIO_PCI_STATUS, status);

    // Read device features
    dev->features = virtio_read32(dev, VIRTIO_PCI_HOST_FEATURES);
    printf("[VirtIO] Device features: 0x%x\n", dev->features);

    // Write guest features (accept all for now)
    virtio_write32(dev, VIRTIO_PCI_GUEST_FEATURES, dev->features);

    // Enable bus mastering
    pci_enable_bus_mastering(pci_dev);

    return 0;
}

// Initialize VirtIO block device
int virtio_blk_init(virtio_device_t* dev) {
    printf("[VirtIO-BLK] Initializing block device...\n");

    // Read configuration
    dev->config.blk.capacity =
        ((uint64_t)virtio_read32(dev, VIRTIO_PCI_CONFIG + 4) << 32) |
        virtio_read32(dev, VIRTIO_PCI_CONFIG);

    printf("[VirtIO-BLK] Capacity: %llu sectors (%llu MB)\n",
           dev->config.blk.capacity,
           (dev->config.blk.capacity * 512) / (1024 * 1024));

    // Create virtqueue 0 (request queue)
    virtqueue_t* vq = virtq_create(dev, 0);
    if (!vq) {
        printf("[VirtIO-BLK] Failed to create request queue\n");
        return -1;
    }
    dev->queues[0] = vq;
    dev->num_queues = 1;

    // Set DRIVER_OK status
    uint8_t status = virtio_read8(dev, VIRTIO_PCI_STATUS);
    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_write8(dev, VIRTIO_PCI_STATUS, status);

    printf("[VirtIO-BLK] Device initialized successfully\n");
    return 0;
}

// Read sectors from VirtIO block device
int virtio_blk_read(virtio_device_t* dev, uint64_t sector, uint32_t count, void* buffer) {
    virtqueue_t* vq = dev->queues[0];
    if (!vq) return -1;

    // Allocate request header
    uint32_t req_phys = pmm_alloc_page();
    virtio_blk_req_header_t* req = (virtio_blk_req_header_t*)req_phys;
    req->type = VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = sector;

    // Allocate status byte
    uint32_t status_phys = pmm_alloc_page();
    uint8_t* status = (uint8_t*)status_phys;
    *status = 0xFF;

    // Get buffer physical address (assuming identity mapping)
    uint32_t buffer_phys = (uint32_t)buffer;

    // Setup descriptors
    uint16_t desc0 = 0;  // Request header (device-readable)
    uint16_t desc1 = 1;  // Data buffer (device-writable)
    uint16_t desc2 = 2;  // Status (device-writable)

    vq->desc[desc0].addr = req_phys;
    vq->desc[desc0].len = sizeof(virtio_blk_req_header_t);
    vq->desc[desc0].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[desc0].next = desc1;

    vq->desc[desc1].addr = buffer_phys;
    vq->desc[desc1].len = count * 512;
    vq->desc[desc1].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    vq->desc[desc1].next = desc2;

    vq->desc[desc2].addr = status_phys;
    vq->desc[desc2].len = 1;
    vq->desc[desc2].flags = VIRTQ_DESC_F_WRITE;
    vq->desc[desc2].next = 0;

    // Add to available ring
    uint16_t avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->queue_size] = desc0;
    vq->avail->idx = avail_idx + 1;

    // Kick device
    virtq_kick(dev, 0);

    // Wait for completion (busy wait)
    while (vq->last_used_idx == vq->used->idx) {
        __asm__ volatile("pause");
    }

    // Get result
    uint32_t len;
    int id = virtq_get_buffer(vq, &len);

    // Check status
    int result = (*status == VIRTIO_BLK_S_OK) ? 0 : -1;

    // Cleanup
    pmm_free_page(req_phys);
    pmm_free_page(status_phys);

    return result;
}

// Write sectors to VirtIO block device
int virtio_blk_write(virtio_device_t* dev, uint64_t sector, uint32_t count, const void* buffer) {
    virtqueue_t* vq = dev->queues[0];
    if (!vq) return -1;

    // Allocate request header
    uint32_t req_phys = pmm_alloc_page();
    virtio_blk_req_header_t* req = (virtio_blk_req_header_t*)req_phys;
    req->type = VIRTIO_BLK_T_OUT;
    req->reserved = 0;
    req->sector = sector;

    // Allocate status byte
    uint32_t status_phys = pmm_alloc_page();
    uint8_t* status = (uint8_t*)status_phys;
    *status = 0xFF;

    // Get buffer physical address
    uint32_t buffer_phys = (uint32_t)buffer;

    // Setup descriptors
    uint16_t desc0 = 0;  // Request header
    uint16_t desc1 = 1;  // Data buffer
    uint16_t desc2 = 2;  // Status

    vq->desc[desc0].addr = req_phys;
    vq->desc[desc0].len = sizeof(virtio_blk_req_header_t);
    vq->desc[desc0].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[desc0].next = desc1;

    vq->desc[desc1].addr = buffer_phys;
    vq->desc[desc1].len = count * 512;
    vq->desc[desc1].flags = VIRTQ_DESC_F_NEXT;  // Read-only (device reads from this)
    vq->desc[desc1].next = desc2;

    vq->desc[desc2].addr = status_phys;
    vq->desc[desc2].len = 1;
    vq->desc[desc2].flags = VIRTQ_DESC_F_WRITE;
    vq->desc[desc2].next = 0;

    // Add to available ring
    uint16_t avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->queue_size] = desc0;
    vq->avail->idx = avail_idx + 1;

    // Kick device
    virtq_kick(dev, 0);

    // Wait for completion
    while (vq->last_used_idx == vq->used->idx) {
        __asm__ volatile("pause");
    }

    // Get result
    uint32_t len;
    int id = virtq_get_buffer(vq, &len);

    // Check status
    int result = (*status == VIRTIO_BLK_S_OK) ? 0 : -1;

    // Cleanup
    pmm_free_page(req_phys);
    pmm_free_page(status_phys);

    return result;
}

// Probe for VirtIO devices
void virtio_probe_devices(void) {
    extern uint32_t pci_get_device_count(void);
    extern pci_device_t* pci_get_device(uint32_t index);

    uint32_t count = pci_get_device_count();

    for (uint32_t i = 0; i < count; i++) {
        pci_device_t* pci_dev = pci_get_device(i);

        // Check if it's a VirtIO device
        if (pci_dev->vendor_id == PCI_VENDOR_ID_REDHAT) {
            // Legacy VirtIO device IDs (0x1000-0x103F)
            // Modern VirtIO device IDs (0x1040+)
            if ((pci_dev->device_id >= 0x1000 && pci_dev->device_id <= 0x103F) ||
                (pci_dev->device_id >= 0x1040 && pci_dev->device_id <= 0x107F)) {

                printf("[VirtIO] Found VirtIO device: 0x%04x\n", pci_dev->device_id);

                // Allocate device structure
                virtio_device_t* dev = (virtio_device_t*)pmm_alloc_page();
                if (!dev) continue;
                mem_set(dev, 0, sizeof(virtio_device_t));

                // Initialize device
                if (virtio_device_init(dev, pci_dev) != 0) {
                    pmm_free_page((uint32_t)dev);
                    continue;
                }

                // Initialize based on device type
                if (pci_dev->device_id == PCI_DEVICE_ID_VIRTIO_BLOCK ||
                    pci_dev->device_id == PCI_DEVICE_ID_VIRTIO_BLOCK_MODERN) {
                    if (virtio_blk_init(dev) == 0) {
                        virtio_devices[virtio_device_count++] = dev;
                    } else {
                        pmm_free_page((uint32_t)dev);
                    }
                }
            }
        }
    }
}

// Initialize VirtIO subsystem
void virtio_init(void) {
    printf("[VirtIO] Initializing VirtIO subsystem...\n");
    virtio_device_count = 0;
    virtio_probe_devices();
    printf("[VirtIO] Found %d VirtIO device(s)\n", virtio_device_count);
}
