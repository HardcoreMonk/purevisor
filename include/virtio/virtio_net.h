/*
 * PureVisor - Virtio Network Device Header
 * 
 * Virtual network device emulation
 */

#ifndef _PUREVISOR_VIRTIO_NET_H
#define _PUREVISOR_VIRTIO_NET_H

#include <lib/types.h>
#include <virtio/virtio.h>

/* ============================================================================
 * Virtio Net Feature Bits
 * ============================================================================ */

#define VIRTIO_NET_F_CSUM               0
#define VIRTIO_NET_F_GUEST_CSUM         1
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2
#define VIRTIO_NET_F_MTU                3
#define VIRTIO_NET_F_MAC                5
#define VIRTIO_NET_F_GUEST_TSO4         7
#define VIRTIO_NET_F_GUEST_TSO6         8
#define VIRTIO_NET_F_GUEST_ECN          9
#define VIRTIO_NET_F_GUEST_UFO          10
#define VIRTIO_NET_F_HOST_TSO4          11
#define VIRTIO_NET_F_HOST_TSO6          12
#define VIRTIO_NET_F_HOST_ECN           13
#define VIRTIO_NET_F_HOST_UFO           14
#define VIRTIO_NET_F_MRG_RXBUF          15
#define VIRTIO_NET_F_STATUS             16
#define VIRTIO_NET_F_CTRL_VQ            17
#define VIRTIO_NET_F_CTRL_RX            18
#define VIRTIO_NET_F_CTRL_VLAN          19
#define VIRTIO_NET_F_GUEST_ANNOUNCE     21
#define VIRTIO_NET_F_MQ                 22
#define VIRTIO_NET_F_CTRL_MAC_ADDR      23
#define VIRTIO_NET_F_SPEED_DUPLEX       63

/* ============================================================================
 * Virtio Net Header
 * ============================================================================ */

#define VIRTIO_NET_HDR_F_NEEDS_CSUM     1
#define VIRTIO_NET_HDR_F_DATA_VALID     2
#define VIRTIO_NET_HDR_F_RSC_INFO       4

#define VIRTIO_NET_HDR_GSO_NONE         0
#define VIRTIO_NET_HDR_GSO_TCPV4        1
#define VIRTIO_NET_HDR_GSO_UDP          3
#define VIRTIO_NET_HDR_GSO_TCPV6        4
#define VIRTIO_NET_HDR_GSO_ECN          0x80

typedef struct PACKED {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;   /* If VIRTIO_NET_F_MRG_RXBUF */
} virtio_net_hdr_t;

/* ============================================================================
 * Virtio Net Configuration
 * ============================================================================ */

#define VIRTIO_NET_S_LINK_UP        1
#define VIRTIO_NET_S_ANNOUNCE       2

typedef struct PACKED {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;             /* In Mbps */
    uint8_t duplex;
} virtio_net_config_t;

/* ============================================================================
 * Network Backend
 * ============================================================================ */

#define NET_PACKET_MAX_SIZE     65536
#define NET_RX_RING_SIZE        256

typedef struct net_packet {
    uint8_t data[NET_PACKET_MAX_SIZE];
    size_t len;
    struct net_packet *next;
} net_packet_t;

typedef struct net_backend {
    /* Backend type */
    enum {
        NET_BACKEND_LOOPBACK,   /* Loopback (for testing) */
        NET_BACKEND_TAP,        /* TAP device */
        NET_BACKEND_USER        /* User-space networking */
    } type;
    
    /* MAC address */
    uint8_t mac[6];
    
    /* RX packet queue */
    net_packet_t *rx_head;
    net_packet_t *rx_tail;
    uint32_t rx_count;
    
    /* TX callback */
    int (*transmit)(struct net_backend *be, const void *data, size_t len);
    
    /* Backend-specific data */
    void *priv;
} net_backend_t;

/* ============================================================================
 * Virtio Net Device
 * ============================================================================ */

typedef struct virtio_net {
    virtio_device_t dev;
    virtio_net_config_t config;
    net_backend_t *backend;
    
    /* Queues */
    virtqueue_t *rx_vq;
    virtqueue_t *tx_vq;
    virtqueue_t *ctrl_vq;
    
    /* Statistics */
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} virtio_net_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * virtio_net_create - Create a virtio network device
 * @backend: Network backend to use
 * 
 * Returns virtio_net device or NULL on failure
 */
virtio_net_t *virtio_net_create(net_backend_t *backend);

/**
 * virtio_net_destroy - Destroy a virtio network device
 * @net: Device to destroy
 */
void virtio_net_destroy(virtio_net_t *net);

/**
 * virtio_net_receive - Queue a received packet
 * @net: Device
 * @data: Packet data
 * @len: Packet length
 */
int virtio_net_receive(virtio_net_t *net, const void *data, size_t len);

/**
 * net_backend_create_loopback - Create loopback backend
 */
net_backend_t *net_backend_create_loopback(void);

/**
 * net_backend_destroy - Destroy network backend
 * @be: Backend to destroy
 */
void net_backend_destroy(net_backend_t *be);

/**
 * Generate random MAC address
 */
void net_generate_mac(uint8_t mac[6]);

#endif /* _PUREVISOR_VIRTIO_NET_H */
