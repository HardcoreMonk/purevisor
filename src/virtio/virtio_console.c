/*
 * PureVisor - Virtio Console Device Implementation
 * 
 * Virtual console (serial) device emulation
 */

#include <lib/types.h>
#include <lib/string.h>
#include <virtio/virtio.h>
#include <virtio/virtio_console.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Ring Buffer Operations
 * ============================================================================ */

static void buffer_init(console_buffer_t *buf)
{
    buf->head = buf->tail = buf->count = 0;
}

static int buffer_put(console_buffer_t *buf, uint8_t c) UNUSED;
static int buffer_put(console_buffer_t *buf, uint8_t c)
{
    if (buf->count >= CONSOLE_BUFFER_SIZE) return -1;
    
    buf->data[buf->tail] = c;
    buf->tail = (buf->tail + 1) % CONSOLE_BUFFER_SIZE;
    buf->count++;
    return 0;
}

static int buffer_get(console_buffer_t *buf, uint8_t *c)
{
    if (buf->count == 0) return -1;
    
    *c = buf->data[buf->head];
    buf->head = (buf->head + 1) % CONSOLE_BUFFER_SIZE;
    buf->count--;
    return 0;
}

static size_t buffer_write(console_buffer_t *buf, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t written = 0;
    
    while (written < len && buf->count < CONSOLE_BUFFER_SIZE) {
        buf->data[buf->tail] = p[written++];
        buf->tail = (buf->tail + 1) % CONSOLE_BUFFER_SIZE;
        buf->count++;
    }
    return written;
}

/* ============================================================================
 * TX Processing (Guest -> Host)
 * ============================================================================ */

static void process_tx(virtio_console_t *con, virtqueue_t *vq, uint16_t head)
{
    virtq_desc_t desc;
    char output_buf[256];
    size_t output_len = 0;
    uint16_t idx = head;
    
    while (1) {
        if (virtq_get_desc(vq, idx, &desc) != 0) break;
        
        if (!(desc.flags & VIRTQ_DESC_F_WRITE)) {
            void *data = phys_to_virt(desc.addr);
            size_t len = desc.len;
            
            /* Copy to output buffer */
            size_t copy = len;
            if (output_len + copy > sizeof(output_buf)) {
                copy = sizeof(output_buf) - output_len;
            }
            memcpy(output_buf + output_len, data, copy);
            output_len += copy;
            
            con->tx_chars += len;
        }
        
        if (!(desc.flags & VIRTQ_DESC_F_NEXT)) break;
        idx = desc.next;
    }
    
    /* Call output handler */
    if (output_len > 0) {
        if (con->output_handler) {
            con->output_handler(con, output_buf, output_len);
        } else {
            /* Default: print to host console */
            for (size_t i = 0; i < output_len; i++) {
                kprintf("%c", output_buf[i]);
            }
        }
    }
    
    virtq_push(vq, head, 0);
}

/* ============================================================================
 * RX Processing (Host -> Guest)
 * ============================================================================ */

static void process_rx(virtio_console_t *con)
{
    virtqueue_t *vq = con->rx_vq;
    console_buffer_t *input = &con->input;
    
    while (input->count > 0) {
        uint16_t head;
        if (virtq_pop(vq, &head) <= 0) break;
        
        virtq_desc_t desc;
        if (virtq_get_desc(vq, head, &desc) != 0) continue;
        
        if (desc.flags & VIRTQ_DESC_F_WRITE) {
            void *buf = phys_to_virt(desc.addr);
            size_t len = 0;
            
            while (len < desc.len && input->count > 0) {
                uint8_t c;
                if (buffer_get(input, &c) == 0) {
                    ((uint8_t *)buf)[len++] = c;
                    con->rx_chars++;
                }
            }
            
            virtq_push(vq, head, len);
        }
    }
    
    if (virtq_should_notify(vq)) {
        con->dev.isr_status |= 1;
    }
}

static int console_queue_notify(virtio_device_t *dev, uint16_t queue)
{
    virtio_console_t *con = (virtio_console_t *)dev;
    
    if (queue == 0) {
        /* RX queue */
        process_rx(con);
    } else if (queue == 1) {
        /* TX queue */
        virtqueue_t *vq = con->tx_vq;
        uint16_t head;
        
        while (virtq_pop(vq, &head) > 0) {
            process_tx(con, vq, head);
        }
        
        if (virtq_should_notify(vq)) {
            dev->isr_status |= 1;
        }
    }
    return 0;
}

static void console_reset(virtio_device_t *dev)
{
    virtio_console_t *con = (virtio_console_t *)dev;
    
    buffer_init(&con->input);
    buffer_init(&con->output);
    con->rx_chars = 0;
    con->tx_chars = 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int virtio_console_write(virtio_console_t *con, const char *data, size_t len)
{
    if (!con) return -1;
    
    size_t written = buffer_write(&con->input, data, len);
    process_rx(con);
    
    return written;
}

void virtio_console_set_handler(virtio_console_t *con,
                                 void (*handler)(virtio_console_t *, const char *, size_t),
                                 void *data)
{
    if (!con) return;
    con->output_handler = handler;
    con->handler_data = data;
}

virtio_console_t *virtio_console_create(void)
{
    virtio_console_t *con = kmalloc(sizeof(virtio_console_t), GFP_KERNEL | GFP_ZERO);
    if (!con) return NULL;
    
    virtio_pci_init(&con->dev, VIRTIO_SUBSYS_CONSOLE);
    
    /* Set console features */
    con->dev.host_features |= BIT(VIRTIO_CONSOLE_F_SIZE);
    
    /* Setup config */
    con->config.cols = 80;
    con->config.rows = 25;
    con->config.max_nr_ports = 1;
    
    virtio_set_config(&con->dev, &con->config, sizeof(con->config));
    
    /* Initialize buffers */
    buffer_init(&con->input);
    buffer_init(&con->output);
    
    /* Add queues: RX (input), TX (output) */
    con->rx_vq = virtio_add_queue(&con->dev, VIRTQ_MAX_SIZE);
    con->tx_vq = virtio_add_queue(&con->dev, VIRTQ_MAX_SIZE);
    
    /* Set callbacks */
    con->dev.queue_notify = console_queue_notify;
    con->dev.reset = console_reset;
    
    pr_info("Virtio-console: Created device (%ux%u)", 
            con->config.cols, con->config.rows);
    
    return con;
}

void virtio_console_destroy(virtio_console_t *con)
{
    if (!con) return;
    pci_unregister_device(&con->dev.pci);
    kfree(con);
}
