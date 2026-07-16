// File: net.c
// Network Stack Core Implementation

#include "net.h"

// External functions
extern void printf(const char* format, ...);
extern uint32_t timer_get_ticks(void);

// Network interfaces
static netif_t* interfaces[4];
static uint32_t interface_count = 0;
static netif_t* default_interface = NULL;

// ARP cache
static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];

// Network statistics
static net_stats_t net_stats = {0};

// IP packet ID counter
static uint16_t ip_id_counter = 0;

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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void mac_copy(mac_addr_t* dest, const mac_addr_t* src) {
    for (int i = 0; i < ETH_ALEN; i++) {
        dest->addr[i] = src->addr[i];
    }
}

int mac_compare(const mac_addr_t* a, const mac_addr_t* b) {
    for (int i = 0; i < ETH_ALEN; i++) {
        if (a->addr[i] != b->addr[i]) return 0;
    }
    return 1;
}

void ip_to_string(ipv4_addr_t ip, char* buf) {
    uint8_t* bytes = (uint8_t*)&ip;
    // Simple integer to string conversion
    for (int i = 0; i < 4; i++) {
        uint8_t byte = bytes[i];
        int hundreds = byte / 100;
        int tens = (byte % 100) / 10;
        int ones = byte % 10;

        if (hundreds) *buf++ = '0' + hundreds;
        if (hundreds || tens) *buf++ = '0' + tens;
        *buf++ = '0' + ones;
        if (i < 3) *buf++ = '.';
    }
    *buf = '\0';
}

// Compute Internet checksum
uint16_t ip_checksum(const void* data, uint32_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(uint8_t*)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

// ============================================================================
// NETWORK INTERFACE MANAGEMENT
// ============================================================================

void net_init(void) {
    printf("[NET] Initializing network stack...\n");

    interface_count = 0;
    default_interface = NULL;
    mem_set(&net_stats, 0, sizeof(net_stats));
    mem_set(arp_cache, 0, sizeof(arp_cache));
    ip_id_counter = 1;

    printf("[NET] Network stack initialized\n");
}

int net_register_interface(netif_t* iface) {
    if (interface_count >= 4) {
        return -1;
    }

    interfaces[interface_count++] = iface;

    if (!default_interface) {
        default_interface = iface;
    }

    char ip_str[16];
    ip_to_string(iface->ip, ip_str);
    printf("[NET] Registered interface %s: IP=%s MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
           iface->name, ip_str,
           iface->mac.addr[0], iface->mac.addr[1], iface->mac.addr[2],
           iface->mac.addr[3], iface->mac.addr[4], iface->mac.addr[5]);

    return 0;
}

netif_t* net_get_default_interface(void) {
    return default_interface;
}

// ============================================================================
// PACKET TRANSMISSION/RECEPTION
// ============================================================================

void net_receive_packet(netif_t* iface, const uint8_t* data, uint32_t len) {
    net_stats.rx_packets++;
    net_stats.rx_bytes += len;

    // Process Ethernet frame
    eth_process_packet(iface, data, len);
}

int net_transmit_packet(netif_t* iface, const uint8_t* data, uint32_t len) {
    if (!iface || !iface->send) {
        return -1;
    }

    int result = iface->send(iface, data, len);
    if (result == 0) {
        net_stats.tx_packets++;
        net_stats.tx_bytes += len;
    } else {
        net_stats.tx_errors++;
    }

    return result;
}

// ============================================================================
// ETHERNET LAYER
// ============================================================================

void eth_process_packet(netif_t* iface, const uint8_t* data, uint32_t len) {
    if (len < ETH_HLEN) {
        return;  // Too small
    }

    const eth_header_t* eth = (const eth_header_t*)data;
    uint16_t eth_type = ntohs(eth->type);
    const uint8_t* payload = data + ETH_HLEN;
    uint32_t payload_len = len - ETH_HLEN;

    switch (eth_type) {
        case ETH_P_ARP:
            arp_process_packet(iface, (const arp_packet_t*)payload);
            break;

        case ETH_P_IP:
            ip_process_packet(iface, payload, payload_len);
            break;

        default:
            // Unknown protocol
            break;
    }
}

int eth_send_packet(netif_t* iface, const mac_addr_t* dest_mac, uint16_t eth_type,
                     const uint8_t* payload, uint32_t payload_len) {
    uint8_t frame[ETH_FRAME_LEN];

    if (ETH_HLEN + payload_len > ETH_FRAME_LEN) {
        return -1;  // Too large
    }

    // Build Ethernet header
    eth_header_t* eth = (eth_header_t*)frame;
    mac_copy(&eth->dest, dest_mac);
    mac_copy(&eth->src, &iface->mac);
    eth->type = htons(eth_type);

    // Copy payload
    mem_copy(frame + ETH_HLEN, payload, payload_len);

    // Send frame
    return net_transmit_packet(iface, frame, ETH_HLEN + payload_len);
}

// ============================================================================
// ARP LAYER
// ============================================================================

void arp_process_packet(netif_t* iface, const arp_packet_t* arp) {
    uint16_t op = ntohs(arp->operation);

    // Update ARP cache
    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == arp->sender_ip) {
            arp_cache[i].ip = arp->sender_ip;
            mac_copy(&arp_cache[i].mac, &arp->sender_mac);
            arp_cache[i].timestamp = timer_get_ticks();
            arp_cache[i].valid = 1;
            break;
        }
    }

    if (op == ARP_REQUEST && arp->target_ip == iface->ip) {
        // ARP request for us, send reply
        arp_send_reply(iface, arp);
    } else if (op == ARP_REPLY) {
        // ARP reply received (cache already updated above)
        char ip_str[16];
        ip_to_string(arp->sender_ip, ip_str);
        printf("[ARP] Resolved %s -> %02x:%02x:%02x:%02x:%02x:%02x\n",
               ip_str,
               arp->sender_mac.addr[0], arp->sender_mac.addr[1],
               arp->sender_mac.addr[2], arp->sender_mac.addr[3],
               arp->sender_mac.addr[4], arp->sender_mac.addr[5]);
    }
}

int arp_resolve(netif_t* iface, ipv4_addr_t ip, mac_addr_t* out_mac) {
    // Check cache
    uint32_t now = timer_get_ticks();
    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            // Check timeout (ARP_CACHE_TIMEOUT * 100 ticks = seconds)
            if (now - arp_cache[i].timestamp < ARP_CACHE_TIMEOUT * 100) {
                mac_copy(out_mac, &arp_cache[i].mac);
                return 0;  // Found in cache
            }
        }
    }

    // Not in cache, send request
    arp_send_request(iface, ip);
    return -1;  // Not resolved yet
}

void arp_send_request(netif_t* iface, ipv4_addr_t target_ip) {
    arp_packet_t arp;
    arp.hw_type = htons(1);  // Ethernet
    arp.proto_type = htons(ETH_P_IP);
    arp.hw_addr_len = 6;
    arp.proto_addr_len = 4;
    arp.operation = htons(ARP_REQUEST);
    mac_copy(&arp.sender_mac, &iface->mac);
    arp.sender_ip = iface->ip;
    mem_set(&arp.target_mac, 0, ETH_ALEN);
    arp.target_ip = target_ip;

    // Broadcast MAC
    mac_addr_t broadcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    eth_send_packet(iface, &broadcast, ETH_P_ARP, (uint8_t*)&arp, sizeof(arp));
}

void arp_send_reply(netif_t* iface, const arp_packet_t* request) {
    arp_packet_t arp;
    arp.hw_type = htons(1);
    arp.proto_type = htons(ETH_P_IP);
    arp.hw_addr_len = 6;
    arp.proto_addr_len = 4;
    arp.operation = htons(ARP_REPLY);
    mac_copy(&arp.sender_mac, &iface->mac);
    arp.sender_ip = iface->ip;
    mac_copy(&arp.target_mac, &request->sender_mac);
    arp.target_ip = request->sender_ip;

    eth_send_packet(iface, &request->sender_mac, ETH_P_ARP, (uint8_t*)&arp, sizeof(arp));
}

// ============================================================================
// IP LAYER
// ============================================================================

void ip_process_packet(netif_t* iface, const uint8_t* data, uint32_t len) {
    if (len < sizeof(ip_header_t)) {
        return;  // Too small
    }

    const ip_header_t* ip = (const ip_header_t*)data;

    // Verify checksum
    uint16_t checksum = ip->checksum;
    ((ip_header_t*)ip)->checksum = 0;
    if (ip_checksum(ip, sizeof(ip_header_t)) != checksum) {
        ((ip_header_t*)ip)->checksum = checksum;
        return;  // Bad checksum
    }
    ((ip_header_t*)ip)->checksum = checksum;

    // Check if packet is for us
    if (ip->dest_ip != iface->ip) {
        return;  // Not for us
    }

    uint8_t ihl = (ip->version_ihl & 0x0F) * 4;
    const uint8_t* payload = data + ihl;
    uint32_t payload_len = ntohs(ip->total_len) - ihl;

    // Process based on protocol
    switch (ip->protocol) {
        case IPPROTO_ICMP:
            icmp_process_packet(iface, ip, payload, payload_len);
            break;

        case IPPROTO_UDP:
            udp_process_packet(iface, ip, payload, payload_len);
            break;

        case IPPROTO_TCP:
            tcp_process_packet(iface, ip, payload, payload_len);
            break;

        default:
            // Unknown protocol
            break;
    }
}

int ip_send_packet(netif_t* iface, ipv4_addr_t dest_ip, uint8_t protocol,
                    const uint8_t* payload, uint32_t payload_len) {
    uint8_t packet[2048];

    if (sizeof(ip_header_t) + payload_len > sizeof(packet)) {
        return -1;  // Too large
    }

    // Build IP header
    ip_header_t* ip = (ip_header_t*)packet;
    ip->version_ihl = 0x45;  // IPv4, IHL=5 (20 bytes)
    ip->tos = 0;
    ip->total_len = htons(sizeof(ip_header_t) + payload_len);
    ip->id = htons(ip_id_counter++);
    ip->frag_offset = 0;
    ip->ttl = IP_DEFAULT_TTL;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = iface->ip;
    ip->dest_ip = dest_ip;

    // Calculate checksum
    ip->checksum = ip_checksum(ip, sizeof(ip_header_t));

    // Copy payload
    mem_copy(packet + sizeof(ip_header_t), payload, payload_len);

    // Resolve MAC address
    mac_addr_t dest_mac;
    if (arp_resolve(iface, dest_ip, &dest_mac) != 0) {
        // MAC not resolved, packet will be dropped
        // In a real implementation, we'd queue the packet
        return -1;
    }

    // Send via Ethernet
    return eth_send_packet(iface, &dest_mac, ETH_P_IP, packet,
                           sizeof(ip_header_t) + payload_len);
}

// ============================================================================
// ICMP LAYER
// ============================================================================

void icmp_process_packet(netif_t* iface, const ip_header_t* ip_hdr,
                         const uint8_t* data, uint32_t len) {
    if (len < sizeof(icmp_header_t)) {
        return;
    }

    const icmp_header_t* icmp = (const icmp_header_t*)data;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        // Ping request, send reply
        char ip_str[16];
        ip_to_string(ip_hdr->src_ip, ip_str);
        printf("[ICMP] Echo request from %s\n", ip_str);

        uint8_t reply[2048];
        icmp_header_t* reply_icmp = (icmp_header_t*)reply;
        reply_icmp->type = ICMP_ECHO_REPLY;
        reply_icmp->code = 0;
        reply_icmp->checksum = 0;
        reply_icmp->id = icmp->id;
        reply_icmp->sequence = icmp->sequence;

        // Copy payload
        uint32_t payload_len = len - sizeof(icmp_header_t);
        mem_copy(reply + sizeof(icmp_header_t), data + sizeof(icmp_header_t), payload_len);

        // Calculate checksum
        reply_icmp->checksum = ip_checksum(reply, sizeof(icmp_header_t) + payload_len);

        // Send reply
        ip_send_packet(iface, ip_hdr->src_ip, IPPROTO_ICMP, reply,
                       sizeof(icmp_header_t) + payload_len);
    } else if (icmp->type == ICMP_ECHO_REPLY) {
        char ip_str[16];
        ip_to_string(ip_hdr->src_ip, ip_str);
        printf("[ICMP] Echo reply from %s\n", ip_str);
    }
}

int icmp_send_echo_request(netif_t* iface, ipv4_addr_t dest_ip,
                            uint16_t id, uint16_t seq) {
    uint8_t packet[64];
    icmp_header_t* icmp = (icmp_header_t*)packet;

    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(seq);

    // Add some data
    for (int i = 0; i < 32; i++) {
        packet[sizeof(icmp_header_t) + i] = i;
    }

    icmp->checksum = ip_checksum(packet, sizeof(icmp_header_t) + 32);

    return ip_send_packet(iface, dest_ip, IPPROTO_ICMP, packet, sizeof(icmp_header_t) + 32);
}

// ============================================================================
// UDP LAYER
// ============================================================================

void udp_process_packet(netif_t* iface, const ip_header_t* ip_hdr,
                        const uint8_t* data, uint32_t len) {
    if (len < sizeof(udp_header_t)) {
        return;
    }

    const udp_header_t* udp = (const udp_header_t*)data;
    uint16_t src_port = ntohs(udp->src_port);
    uint16_t dest_port = ntohs(udp->dest_port);

    char ip_str[16];
    ip_to_string(ip_hdr->src_ip, ip_str);
    printf("[UDP] Packet from %s:%d to port %d\n", ip_str, src_port, dest_port);

    // TODO: Socket delivery
}

int udp_send_packet(netif_t* iface, ipv4_addr_t dest_ip,
                    uint16_t src_port, uint16_t dest_port,
                    const uint8_t* data, uint32_t len) {
    uint8_t packet[2048];

    if (sizeof(udp_header_t) + len > sizeof(packet)) {
        return -1;
    }

    udp_header_t* udp = (udp_header_t*)packet;
    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons(sizeof(udp_header_t) + len);
    udp->checksum = 0;  // Optional for IPv4

    mem_copy(packet + sizeof(udp_header_t), data, len);

    return ip_send_packet(iface, dest_ip, IPPROTO_UDP, packet,
                          sizeof(udp_header_t) + len);
}

// ============================================================================
// TCP LAYER (Stub)
// ============================================================================

void tcp_process_packet(netif_t* iface, const ip_header_t* ip_hdr,
                        const uint8_t* data, uint32_t len) {
    if (len < sizeof(tcp_header_t)) {
        return;
    }

    const tcp_header_t* tcp = (const tcp_header_t*)data;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dest_port = ntohs(tcp->dest_port);

    char ip_str[16];
    ip_to_string(ip_hdr->src_ip, ip_str);
    printf("[TCP] Packet from %s:%d to port %d (flags=0x%02x)\n",
           ip_str, src_port, dest_port, tcp->flags);

    // TODO: TCP state machine
}
