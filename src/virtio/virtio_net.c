/*
 * PureVisor - Virtio Network Device Implementation
 * 
 * Virtual network device emulation
 */

#include <lib/types.h>
#include <lib/string.h>
#include <virtio/virtio.h>
#include <virtio/virtio_net.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * MAC Address Generation
 * ============================================================================ */

void net_generate_mac(uint8_t mac[6])
{
    uint64_t tsc = rdtsc();
    
    mac[0] = 0x52;  /* Locally administered, unicast */
    mac[1] = 0x54;
    mac[2] = 0x00;
    mac[3] = (tsc >> 8) & 0xFF;
    mac[4] = (tsc >> 16) & 0xFF;
    mac[5] = (tsc >> 24) & 0xFF;
}

/* ============================================================================
 * Loopback Backend
 * ============================================================================ */

static int loopback_transmit(net_backend_t *be, const void *data, size_t len)
{
    if (len > NET_PACKET_MAX_SIZE) return -1;
    
    net_packet_t *pkt = kmalloc(sizeof(net_packet_t), GFP_KERNEL);
    if (!pkt) return -1;
    
    memcpy(pkt->data, data, len);
    pkt->len = len;
    pkt->next = NULL;
    
    if (be->rx_tail) {
        be->rx_tail->next = pkt;
        be->rx_tail = pkt;
    } else {
        be->rx_head = be->rx_tail = pkt;
    }
    be->rx_count++;
    
    return 0;
}

net_backend_t *net_backend_create_loopback(void)
{
    net_backend_t *be = kmalloc(sizeof(net_backend_t), GFP_KERNEL | GFP_ZERO);
    if (!be) return NULL;
    
    be->type = NET_BACKEND_LOOPBACK;
    net_generate_mac(be->mac);
    be->transmit = loopback_transmit;
    
    pr_info("Net: Loopback, MAC=%02x:%02x:%02x:%02x:%02x:%02x",
            be->mac[0], be->mac[1], be->mac[2],
            be->mac[3], be->mac[4], be->mac[5]);
    
    return be;
}

void net_backend_destroy(net_backend_t *be)
{
    if (!be) return;
    
    net_packet_t *pkt = be->rx_head;
    while (pkt) {
        net_packet_t *next = pkt->next;
        kfree(pkt);
        pkt = next;
    }
    kfree(be);
}

/* ============================================================================
 * TX/RX Processing
 * ============================================================================ */

static void process_tx(virtio_net_t *net, virtqueue_t *vq, uint16_t head)
{
    net_backend_t *be = net->backend;
    virtq_desc_t desc;
    uint8_t packet[NET_PACKET_MAX_SIZE];
    size_t packet_len = 0;
    bool first = true;
    uint16_t idx = head;
    
    while (1) {
        if (virtq_get_desc(vq, idx, &desc) != 0) break;
        
        if (!(desc.flags & VIRTQ_DESC_F_WRITE)) {
            void *src = phys_to_virt(desc.addr);
            size_t copy_len = desc.len;
            
            if (first) {
                first = false;
                if (desc.len > sizeof(virtio_net_hdr_t)) {
                    src = (uint8_t *)src + sizeof(virtio_net_hdr_t);
                    copy_len -= sizeof(virtio_net_hdr_t);
                } else {
                    copy_len = 0;
                }
            }
            
            if (packet_len + copy_len > NET_PACKET_MAX_SIZE) {
                copy_len = NET_PACKET_MAX_SIZE - packet_len;
            }
            if (copy_len > 0) {
                memcpy(packet + packet_len, src, copy_len);
                packet_len += copy_len;
            }
        }
        
        if (!(desc.flags & VIRTQ_DESC_F_NEXT)) break;
        idx = desc.next;
    }
    
    if (packet_len > 0 && be->transmit) {
        be->transmit(be, packet, packet_len);
        net->tx_packets++;
        net->tx_bytes += packet_len;
    }
    
    virtq_push(vq, head, 0);
}

static void process_rx(virtio_net_t *net)
{
    net_backend_t *be = net->backend;
    virtqueue_t *vq = net->rx_vq;
    
    while (be->rx_head) {
        uint16_t head;
        if (virtq_pop(vq, &head) <= 0) break;
        
        net_packet_t *pkt = be->rx_head;
        be->rx_head = pkt->next;
        if (!be->rx_head) be->rx_tail = NULL;
        be->rx_count--;
        
        virtq_desc_t desc;
        if (virtq_get_desc(vq, head, &desc) == 0 && (desc.flags & VIRTQ_DESC_F_WRITE)) {
            void *buf = phys_to_virt(desc.addr);
            virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)buf;
            memset(hdr, 0, sizeof(*hdr));
            
            size_t hdr_len = sizeof(virtio_net_hdr_t);
            size_t copy_len = pkt->len;
            if (hdr_len + copy_len > desc.len) copy_len = desc.len - hdr_len;
            memcpy((uint8_t *)buf + hdr_len, pkt->data, copy_len);
            
            virtq_push(vq, head, hdr_len + copy_len);
            net->rx_packets++;
            net->rx_bytes += copy_len;
        }
        kfree(pkt);
    }
    
    if (virtq_should_notify(vq)) {
        net->dev.isr_status |= 1;
    }
}

static int net_queue_notify(virtio_device_t *dev, uint16_t queue)
{
    virtio_net_t *net = (virtio_net_t *)dev;
    
    if (queue == 0) {
        process_rx(net);
    } else if (queue == 1) {
        virtqueue_t *vq = net->tx_vq;
        uint16_t head;
        while (virtq_pop(vq, &head) > 0) {
            process_tx(net, vq, head);
        }
        if (virtq_should_notify(vq)) {
            dev->isr_status |= 1;
        }
    }
    return 0;
}

static void net_reset(virtio_device_t *dev)
{
    virtio_net_t *net = (virtio_net_t *)dev;
    net->rx_packets = net->tx_packets = 0;
    net->rx_bytes = net->tx_bytes = 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int virtio_net_receive(virtio_net_t *net, const void *data, size_t len)
{
    if (!net || !net->backend) return -1;
    
    net_backend_t *be = net->backend;
    net_packet_t *pkt = kmalloc(sizeof(net_packet_t), GFP_KERNEL);
    if (!pkt) return -1;
    
    size_t copy_len = len > NET_PACKET_MAX_SIZE ? NET_PACKET_MAX_SIZE : len;
    memcpy(pkt->data, data, copy_len);
    pkt->len = copy_len;
    pkt->next = NULL;
    
    if (be->rx_tail) {
        be->rx_tail->next = pkt;
        be->rx_tail = pkt;
    } else {
        be->rx_head = be->rx_tail = pkt;
    }
    be->rx_count++;
    
    process_rx(net);
    return 0;
}

virtio_net_t *virtio_net_create(net_backend_t *backend)
{
    if (!backend) return NULL;
    
    virtio_net_t *net = kmalloc(sizeof(virtio_net_t), GFP_KERNEL | GFP_ZERO);
    if (!net) return NULL;
    
    virtio_pci_init(&net->dev, VIRTIO_SUBSYS_NET);
    net->backend = backend;
    
    net->dev.host_features |= BIT(VIRTIO_NET_F_MAC) |
                               BIT(VIRTIO_NET_F_STATUS) |
                               BIT(VIRTIO_NET_F_MRG_RXBUF);
    
    memcpy(net->config.mac, backend->mac, 6);
    net->config.status = VIRTIO_NET_S_LINK_UP;
    net->config.max_virtqueue_pairs = 1;
    net->config.mtu = 1500;
    
    virtio_set_config(&net->dev, &net->config, sizeof(net->config));
    
    net->rx_vq = virtio_add_queue(&net->dev, VIRTQ_MAX_SIZE);
    net->tx_vq = virtio_add_queue(&net->dev, VIRTQ_MAX_SIZE);
    
    net->dev.queue_notify = net_queue_notify;
    net->dev.reset = net_reset;
    
    pr_info("Virtio-net: MAC=%02x:%02x:%02x:%02x:%02x:%02x",
            net->config.mac[0], net->config.mac[1], net->config.mac[2],
            net->config.mac[3], net->config.mac[4], net->config.mac[5]);
    
    return net;
}

void virtio_net_destroy(virtio_net_t *net)
{
    if (!net) return;
    pci_unregister_device(&net->dev.pci);
    kfree(net);
}
