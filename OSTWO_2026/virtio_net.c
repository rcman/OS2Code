// File: virtio_net.c
// VirtIO Network Device Driver Implementation

#include "virtio_net.h"
#include "pmm.h"

// External functions
extern void printf(const char* format, ...);
extern uint32_t pmm_alloc_page(void);
extern void pmm_free_page(uint32_t addr);
extern int net_register_interface(netif_t* iface);
extern void net_receive_packet(netif_t* iface, const uint8_t* data, uint32_t len);

// VirtIO-Net devices
static virtio_net_device_t* virtio_net_devices[4];
static uint32_t virtio_net_device_count = 0;

// Simple memory copy
static void mem_copy(void* dest, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

// Simple memory set
static void mem_set(void* dest, uint8_t val, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = val;
    }
}

// Simple string copy
static void str_copy(char* dest, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Send packet via VirtIO-Net
int virtio_net_send(netif_t* iface, const uint8_t* data, uint32_t len) {
    virtio_net_device_t* netdev = (virtio_net_device_t*)iface->driver_data;
    virtio_device_t* vdev = netdev->vdev;
    virtqueue_t* vq = vdev->queues[1];  // TX queue

    if (!vq || len > ETH_FRAME_LEN) {
        return -1;
    }

    // Build packet with VirtIO-Net header
    virtio_net_hdr_t* hdr = (virtio_net_hdr_t*)netdev->tx_buffer;
    mem_set(hdr, 0, sizeof(virtio_net_hdr_t));

    // Copy packet data after header
    mem_copy(netdev->tx_buffer + sizeof(virtio_net_hdr_t), data, len);

    uint32_t total_len = sizeof(virtio_net_hdr_t) + len;

    // Setup descriptor
    vq->desc[0].addr = (uint32_t)netdev->tx_buffer;
    vq->desc[0].len = total_len;
    vq->desc[0].flags = 0;  // Device-readable

    // Add to available ring
    uint16_t avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->queue_size] = 0;
    vq->avail->idx = avail_idx + 1;

    // Notify device
    extern void virtq_kick(virtio_device_t* dev, uint32_t queue_index);
    virtq_kick(vdev, 1);

    return 0;
}

// Receive packets from VirtIO-Net
void virtio_net_receive(virtio_net_device_t* netdev) {
    virtio_device_t* vdev = netdev->vdev;
    virtqueue_t* vq = vdev->queues[0];  // RX queue

    if (!vq) return;

    // Check for completed buffers
    while (vq->last_used_idx != vq->used->idx) {
        extern int virtq_get_buffer(virtqueue_t* vq, uint32_t* len);
        uint32_t len;
        int desc_id = virtq_get_buffer(vq, &len);

        if (desc_id >= 0 && desc_id < 16) {
            // Skip VirtIO-Net header
            uint8_t* packet_data = netdev->rx_buffers[desc_id] + sizeof(virtio_net_hdr_t);
            uint32_t packet_len = len - sizeof(virtio_net_hdr_t);

            // Process packet
            net_receive_packet(&netdev->netif, packet_data, packet_len);

            // Re-add buffer to RX queue
            vq->desc[desc_id].addr = (uint32_t)netdev->rx_buffers[desc_id];
            vq->desc[desc_id].len = 2048;
            vq->desc[desc_id].flags = VIRTQ_DESC_F_WRITE;  // Device-writable

            uint16_t avail_idx = vq->avail->idx;
            vq->avail->ring[avail_idx % vq->queue_size] = desc_id;
            vq->avail->idx = avail_idx + 1;
        }
    }

    // Kick RX queue to notify device
    extern void virtq_kick(virtio_device_t* dev, uint32_t queue_index);
    virtq_kick(vdev, 0);
}

// Poll for received packets
static int virtio_net_recv(netif_t* iface, uint8_t* data, uint32_t max_len) {
    virtio_net_device_t* netdev = (virtio_net_device_t*)iface->driver_data;
    virtio_net_receive(netdev);
    return 0;  // Packets are processed in virtio_net_receive
}

// Initialize VirtIO-Net device
int virtio_net_device_init(virtio_net_device_t* netdev, virtio_device_t* vdev) {
    printf("[VirtIO-Net] Initializing network device...\n");

    netdev->vdev = vdev;

    // Read MAC address from device config
    extern uint8_t virtio_read8(virtio_device_t* dev, uint32_t offset);
    for (int i = 0; i < 6; i++) {
        // VirtIO-Net config starts at offset 20
        extern uint32_t inb(uint16_t port);
        netdev->mac.addr[i] = inb(vdev->io_base + 20 + i);
    }

    printf("[VirtIO-Net] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           netdev->mac.addr[0], netdev->mac.addr[1], netdev->mac.addr[2],
           netdev->mac.addr[3], netdev->mac.addr[4], netdev->mac.addr[5]);

    // Create RX queue (queue 0)
    extern virtqueue_t* virtq_create(virtio_device_t* dev, uint32_t queue_index);
    virtqueue_t* rx_vq = virtq_create(vdev, 0);
    if (!rx_vq) {
        printf("[VirtIO-Net] Failed to create RX queue\n");
        return -1;
    }
    vdev->queues[0] = rx_vq;

    // Create TX queue (queue 1)
    virtqueue_t* tx_vq = virtq_create(vdev, 1);
    if (!tx_vq) {
        printf("[VirtIO-Net] Failed to create TX queue\n");
        return -1;
    }
    vdev->queues[1] = tx_vq;
    vdev->num_queues = 2;

    // Fill RX queue with buffers
    for (uint16_t i = 0; i < 16; i++) {
        rx_vq->desc[i].addr = (uint32_t)netdev->rx_buffers[i];
        rx_vq->desc[i].len = 2048;
        rx_vq->desc[i].flags = VIRTQ_DESC_F_WRITE;  // Device-writable
        rx_vq->desc[i].next = 0;

        // Add to available ring
        rx_vq->avail->ring[i] = i;
    }
    rx_vq->avail->idx = 16;

    // Kick RX queue
    extern void virtq_kick(virtio_device_t* dev, uint32_t queue_index);
    virtq_kick(vdev, 0);

    // Set DRIVER_OK status
    extern void virtio_write8(virtio_device_t* dev, uint32_t offset, uint8_t value);
    extern uint8_t virtio_read8(virtio_device_t* dev, uint32_t offset);
    uint8_t status = virtio_read8(vdev, 18);  // VIRTIO_PCI_STATUS
    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_write8(vdev, 18, status);

    // Setup network interface
    str_copy(netdev->netif.name, "virtio0", sizeof(netdev->netif.name));
    mem_copy(&netdev->netif.mac, &netdev->mac, sizeof(mac_addr_t));

    // Set IP address (hardcoded for now - should be configured)
    // 10.0.2.15 is QEMU's default guest IP
    netdev->netif.ip = (10 << 0) | (0 << 8) | (2 << 16) | (15 << 24);
    netdev->netif.netmask = (255 << 0) | (255 << 8) | (255 << 16) | (0 << 24);
    netdev->netif.gateway = (10 << 0) | (0 << 8) | (2 << 16) | (2 << 24);
    netdev->netif.mtu = ETH_DATA_LEN;
    netdev->netif.driver_data = netdev;
    netdev->netif.send = virtio_net_send;
    netdev->netif.recv = virtio_net_recv;

    // Register with network stack
    net_register_interface(&netdev->netif);

    printf("[VirtIO-Net] Device initialized successfully\n");
    return 0;
}

// Initialize VirtIO-Net driver
void virtio_net_init(void) {
    printf("[VirtIO-Net] Initializing VirtIO-Net driver...\n");

    extern uint32_t pci_get_device_count(void);
    extern pci_device_t* pci_get_device(uint32_t index);
    extern int virtio_device_init(virtio_device_t* dev, pci_device_t* pci_dev);

    uint32_t count = pci_get_device_count();

    for (uint32_t i = 0; i < count; i++) {
        pci_device_t* pci_dev = pci_get_device(i);

        // Check for VirtIO-Net device
        if (pci_dev->vendor_id == PCI_VENDOR_ID_REDHAT &&
            (pci_dev->device_id == PCI_DEVICE_ID_VIRTIO_NET ||
             pci_dev->device_id == PCI_DEVICE_ID_VIRTIO_NET_MODERN)) {

            printf("[VirtIO-Net] Found VirtIO-Net device\n");

            // Allocate device structure
            virtio_device_t* vdev = (virtio_device_t*)pmm_alloc_page();
            if (!vdev) continue;
            mem_set(vdev, 0, sizeof(virtio_device_t));

            // Initialize VirtIO device
            if (virtio_device_init(vdev, pci_dev) != 0) {
                pmm_free_page((uint32_t)vdev);
                continue;
            }

            // Allocate VirtIO-Net device structure
            virtio_net_device_t* netdev = (virtio_net_device_t*)pmm_alloc_page();
            if (!netdev) {
                pmm_free_page((uint32_t)vdev);
                continue;
            }
            mem_set(netdev, 0, sizeof(virtio_net_device_t));

            // Initialize VirtIO-Net device
            if (virtio_net_device_init(netdev, vdev) == 0) {
                virtio_net_devices[virtio_net_device_count++] = netdev;
            } else {
                pmm_free_page((uint32_t)netdev);
                pmm_free_page((uint32_t)vdev);
            }
        }
    }

    printf("[VirtIO-Net] Found %d VirtIO-Net device(s)\n", virtio_net_device_count);
}

// Poll all VirtIO-Net devices for received packets
void virtio_net_poll_all(void) {
    for (uint32_t i = 0; i < virtio_net_device_count; i++) {
        virtio_net_receive(virtio_net_devices[i]);
    }
}
