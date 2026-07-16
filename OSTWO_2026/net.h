// File: net.h
// Network Stack Core Definitions

#ifndef NET_H
#define NET_H

#include "types.h"

// Ethernet constants
#define ETH_ALEN            6       // Ethernet address length
#define ETH_HLEN            14      // Ethernet header length
#define ETH_ZLEN            60      // Min frame size
#define ETH_FRAME_LEN       1514    // Max frame size
#define ETH_DATA_LEN        1500    // Max payload size (MTU)

// Ethernet types
#define ETH_P_IP            0x0800  // IPv4
#define ETH_P_ARP           0x0806  // ARP
#define ETH_P_IPV6          0x86DD  // IPv6

// IP protocol numbers
#define IPPROTO_ICMP        1
#define IPPROTO_TCP         6
#define IPPROTO_UDP         17

// ARP constants
#define ARP_REQUEST         1
#define ARP_REPLY           2
#define ARP_CACHE_SIZE      32
#define ARP_CACHE_TIMEOUT   300     // 5 minutes in seconds

// IP constants
#define IP_VERSION          4
#define IP_DEFAULT_TTL      64
#define IP_FRAG_OFFSET_MASK 0x1FFF
#define IP_FLAG_DF          0x4000  // Don't fragment
#define IP_FLAG_MF          0x2000  // More fragments

// ICMP types
#define ICMP_ECHO_REPLY     0
#define ICMP_DEST_UNREACH   3
#define ICMP_ECHO_REQUEST   8
#define ICMP_TIME_EXCEEDED  11

// TCP flags
#define TCP_FLAG_FIN        0x01
#define TCP_FLAG_SYN        0x02
#define TCP_FLAG_RST        0x04
#define TCP_FLAG_PSH        0x08
#define TCP_FLAG_ACK        0x10
#define TCP_FLAG_URG        0x20

// TCP states
#define TCP_CLOSED          0
#define TCP_LISTEN          1
#define TCP_SYN_SENT        2
#define TCP_SYN_RECEIVED    3
#define TCP_ESTABLISHED     4
#define TCP_FIN_WAIT_1      5
#define TCP_FIN_WAIT_2      6
#define TCP_CLOSE_WAIT      7
#define TCP_CLOSING         8
#define TCP_LAST_ACK        9
#define TCP_TIME_WAIT       10

// Socket types
#define SOCK_STREAM         1       // TCP
#define SOCK_DGRAM          2       // UDP
#define SOCK_RAW            3       // Raw

// Address families
#define AF_INET             2       // IPv4

// Network byte order conversion.
// These are inline functions rather than macros: the old macros
// evaluated their argument multiple times, so expressions with side
// effects such as htons(ip_id_counter++) were undefined behavior
// (the counter was incremented 2-3 times per call).
static inline uint16_t htons(uint16_t x) {
    return (uint16_t)((x << 8) | (x >> 8));
}
static inline uint16_t ntohs(uint16_t x) { return htons(x); }

static inline uint32_t htonl(uint32_t x) {
    return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) |
           ((x & 0x00FF0000u) >> 8)  | ((x & 0xFF000000u) >> 24);
}
static inline uint32_t ntohl(uint32_t x) { return htonl(x); }

// MAC address structure
typedef struct {
    uint8_t addr[ETH_ALEN];
} __attribute__((packed)) mac_addr_t;

// IPv4 address structure
typedef uint32_t ipv4_addr_t;

// Ethernet header
typedef struct {
    mac_addr_t dest;
    mac_addr_t src;
    uint16_t type;
} __attribute__((packed)) eth_header_t;

// ARP packet
typedef struct {
    uint16_t hw_type;           // Hardware type (Ethernet = 1)
    uint16_t proto_type;        // Protocol type (IPv4 = 0x0800)
    uint8_t hw_addr_len;        // Hardware address length (6)
    uint8_t proto_addr_len;     // Protocol address length (4)
    uint16_t operation;         // ARP operation (request/reply)
    mac_addr_t sender_mac;
    ipv4_addr_t sender_ip;
    mac_addr_t target_mac;
    ipv4_addr_t target_ip;
} __attribute__((packed)) arp_packet_t;

// IP header
typedef struct {
    uint8_t version_ihl;        // Version (4 bits) + IHL (4 bits)
    uint8_t tos;                // Type of service
    uint16_t total_len;         // Total length
    uint16_t id;                // Identification
    uint16_t frag_offset;       // Flags (3 bits) + Fragment offset (13 bits)
    uint8_t ttl;                // Time to live
    uint8_t protocol;           // Protocol (TCP=6, UDP=17, ICMP=1)
    uint16_t checksum;          // Header checksum
    ipv4_addr_t src_ip;
    ipv4_addr_t dest_ip;
} __attribute__((packed)) ip_header_t;

// ICMP header
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

// UDP header
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

// TCP header
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset;        // Data offset (4 bits) + reserved (4 bits)
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

// Network packet buffer
typedef struct {
    uint8_t data[2048];         // Packet data
    uint32_t len;               // Packet length
    uint32_t offset;            // Current offset for reading
} netbuf_t;

// ARP cache entry
typedef struct {
    ipv4_addr_t ip;
    mac_addr_t mac;
    uint32_t timestamp;
    uint8_t valid;
} arp_cache_entry_t;

// Socket address structure (like sockaddr_in)
typedef struct {
    uint16_t family;            // AF_INET
    uint16_t port;              // Port number (network byte order)
    ipv4_addr_t addr;           // IP address (network byte order)
    uint8_t zero[8];            // Padding
} sockaddr_in_t;

// Forward declaration
typedef struct netif_s netif_t;

// Network interface structure
struct netif_s {
    char name[16];
    mac_addr_t mac;
    ipv4_addr_t ip;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    uint32_t mtu;
    void* driver_data;          // Driver-specific data

    // Function pointers for device operations
    int (*send)(netif_t* iface, const uint8_t* data, uint32_t len);
    int (*recv)(netif_t* iface, uint8_t* data, uint32_t max_len);
};

// Network statistics
typedef struct {
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_errors;
    uint32_t tx_errors;
    uint32_t rx_dropped;
    uint32_t tx_dropped;
} net_stats_t;

// Initialize network stack
void net_init(void);

// Register network interface
int net_register_interface(netif_t* iface);

// Get default network interface
netif_t* net_get_default_interface(void);

// Receive packet (called by driver)
void net_receive_packet(netif_t* iface, const uint8_t* data, uint32_t len);

// Transmit packet
int net_transmit_packet(netif_t* iface, const uint8_t* data, uint32_t len);

// Ethernet layer
void eth_process_packet(netif_t* iface, const uint8_t* data, uint32_t len);
int eth_send_packet(netif_t* iface, const mac_addr_t* dest_mac, uint16_t eth_type,
                     const uint8_t* payload, uint32_t payload_len);

// ARP layer
void arp_process_packet(netif_t* iface, const arp_packet_t* arp);
int arp_resolve(netif_t* iface, ipv4_addr_t ip, mac_addr_t* out_mac);
void arp_send_request(netif_t* iface, ipv4_addr_t target_ip);
void arp_send_reply(netif_t* iface, const arp_packet_t* request);

// IP layer
void ip_process_packet(netif_t* iface, const uint8_t* data, uint32_t len);
int ip_send_packet(netif_t* iface, ipv4_addr_t dest_ip, uint8_t protocol,
                    const uint8_t* payload, uint32_t payload_len);
uint16_t ip_checksum(const void* data, uint32_t len);

// ICMP layer
void icmp_process_packet(netif_t* iface, const ip_header_t* ip_hdr,
                         const uint8_t* data, uint32_t len);
int icmp_send_echo_request(netif_t* iface, ipv4_addr_t dest_ip,
                            uint16_t id, uint16_t seq);

// UDP layer
void udp_process_packet(netif_t* iface, const ip_header_t* ip_hdr,
                        const uint8_t* data, uint32_t len);
int udp_send_packet(netif_t* iface, ipv4_addr_t dest_ip,
                    uint16_t src_port, uint16_t dest_port,
                    const uint8_t* data, uint32_t len);

// TCP layer
void tcp_process_packet(netif_t* iface, const ip_header_t* ip_hdr,
                        const uint8_t* data, uint32_t len);

// Utility functions
void mac_copy(mac_addr_t* dest, const mac_addr_t* src);
int mac_compare(const mac_addr_t* a, const mac_addr_t* b);
void ip_to_string(ipv4_addr_t ip, char* buf);
ipv4_addr_t string_to_ip(const char* str);

#endif // NET_H
