/*
 * PureVisor - Virtio Console Device Header
 * 
 * Virtual console (serial) device emulation
 */

#ifndef _PUREVISOR_VIRTIO_CONSOLE_H
#define _PUREVISOR_VIRTIO_CONSOLE_H

#include <lib/types.h>
#include <virtio/virtio.h>

/* ============================================================================
 * Virtio Console Feature Bits
 * ============================================================================ */

#define VIRTIO_CONSOLE_F_SIZE           0
#define VIRTIO_CONSOLE_F_MULTIPORT      1
#define VIRTIO_CONSOLE_F_EMERG_WRITE    2

/* ============================================================================
 * Virtio Console Configuration
 * ============================================================================ */

typedef struct PACKED {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} virtio_console_config_t;

/* ============================================================================
 * Console Ring Buffer
 * ============================================================================ */

#define CONSOLE_BUFFER_SIZE     4096

typedef struct {
    uint8_t data[CONSOLE_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} console_buffer_t;

/* ============================================================================
 * Virtio Console Device
 * ============================================================================ */

typedef struct virtio_console {
    virtio_device_t dev;
    virtio_console_config_t config;
    
    /* Queues */
    virtqueue_t *rx_vq;     /* Host -> Guest (input) */
    virtqueue_t *tx_vq;     /* Guest -> Host (output) */
    
    /* Buffers */
    console_buffer_t input;
    console_buffer_t output;
    
    /* Callbacks */
    void (*output_handler)(struct virtio_console *con, const char *data, size_t len);
    void *handler_data;
    
    /* Statistics */
    uint64_t rx_chars;
    uint64_t tx_chars;
} virtio_console_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * virtio_console_create - Create a virtio console device
 */
virtio_console_t *virtio_console_create(void);

/**
 * virtio_console_destroy - Destroy a virtio console device
 */
void virtio_console_destroy(virtio_console_t *con);

/**
 * virtio_console_write - Write data to console (host -> guest)
 * @con: Console device
 * @data: Data to write
 * @len: Length of data
 */
int virtio_console_write(virtio_console_t *con, const char *data, size_t len);

/**
 * virtio_console_set_handler - Set output handler
 * @con: Console device
 * @handler: Callback for guest output
 * @data: User data for callback
 */
void virtio_console_set_handler(virtio_console_t *con,
                                 void (*handler)(virtio_console_t *, const char *, size_t),
                                 void *data);

#endif /* _PUREVISOR_VIRTIO_CONSOLE_H */
