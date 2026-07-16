// File: virtio_net.h
// VirtIO Network Device Driver

#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include "types.h"
#include "virtio.h"
#include "net.h"

// VirtIO-Net feature bits
#define VIRTIO_NET_F_CSUM           (1 << 0)
#define VIRTIO_NET_F_GUEST_CSUM     (1 << 1)
#define VIRTIO_NET_F_MAC            (1 << 5)
#define VIRTIO_NET_F_GSO            (1 << 6)
#define VIRTIO_NET_F_GUEST_TSO4     (1 << 7)
#define VIRTIO_NET_F_GUEST_TSO6     (1 << 8)
#define VIRTIO_NET_F_GUEST_ECN      (1 << 9)
#define VIRTIO_NET_F_GUEST_UFO      (1 << 10)
#define VIRTIO_NET_F_HOST_TSO4      (1 << 11)
#define VIRTIO_NET_F_HOST_TSO6      (1 << 12)
#define VIRTIO_NET_F_HOST_ECN       (1 << 13)
#define VIRTIO_NET_F_HOST_UFO       (1 << 14)
#define VIRTIO_NET_F_MRG_RXBUF      (1 << 15)
#define VIRTIO_NET_F_STATUS         (1 << 16)
#define VIRTIO_NET_F_CTRL_VQ        (1 << 17)

// VirtIO-Net header
typedef struct {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;   // Only if VIRTIO_NET_F_MRG_RXBUF
} __attribute__((packed)) virtio_net_hdr_t;

// VirtIO-Net device structure
typedef struct {
    virtio_device_t* vdev;
    netif_t netif;
    mac_addr_t mac;

    // Receive/transmit buffers
    uint8_t rx_buffers[16][2048];
    uint8_t tx_buffer[2048];
} virtio_net_device_t;

// Initialize VirtIO-Net driver
void virtio_net_init(void);

// Initialize a VirtIO-Net device
int virtio_net_device_init(virtio_net_device_t* netdev, virtio_device_t* vdev);

// Send packet
int virtio_net_send(netif_t* iface, const uint8_t* data, uint32_t len);

// Receive packet
void virtio_net_receive(virtio_net_device_t* netdev);

#endif // VIRTIO_NET_H
