/*
 * PureVisor - Virtio Block Device Header
 * 
 * Virtual block device emulation
 */

#ifndef _PUREVISOR_VIRTIO_BLK_H
#define _PUREVISOR_VIRTIO_BLK_H

#include <lib/types.h>
#include <virtio/virtio.h>

/* ============================================================================
 * Virtio Block Feature Bits
 * ============================================================================ */

#define VIRTIO_BLK_F_SIZE_MAX       1
#define VIRTIO_BLK_F_SEG_MAX        2
#define VIRTIO_BLK_F_GEOMETRY       4
#define VIRTIO_BLK_F_RO             5
#define VIRTIO_BLK_F_BLK_SIZE       6
#define VIRTIO_BLK_F_FLUSH          9
#define VIRTIO_BLK_F_TOPOLOGY       10
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_DISCARD        13
#define VIRTIO_BLK_F_WRITE_ZEROES   14

/* ============================================================================
 * Virtio Block Request Types
 * ============================================================================ */

#define VIRTIO_BLK_T_IN             0   /* Read */
#define VIRTIO_BLK_T_OUT            1   /* Write */
#define VIRTIO_BLK_T_FLUSH          4
#define VIRTIO_BLK_T_GET_ID         8
#define VIRTIO_BLK_T_DISCARD        11
#define VIRTIO_BLK_T_WRITE_ZEROES   13

/* ============================================================================
 * Virtio Block Status
 * ============================================================================ */

#define VIRTIO_BLK_S_OK             0
#define VIRTIO_BLK_S_IOERR          1
#define VIRTIO_BLK_S_UNSUPP         2

/* ============================================================================
 * Virtio Block Configuration
 * ============================================================================ */

typedef struct PACKED {
    uint64_t capacity;          /* Size in 512-byte sectors */
    uint32_t size_max;          /* Max segment size (if SIZE_MAX) */
    uint32_t seg_max;           /* Max segments per request (if SEG_MAX) */
    struct PACKED {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;                 /* Disk geometry (if GEOMETRY) */
    uint32_t blk_size;          /* Block size (if BLK_SIZE) */
    struct PACKED {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;                 /* Topology info (if TOPOLOGY) */
    uint8_t writeback;          /* Writeback mode (if CONFIG_WCE) */
    uint8_t unused0;
    uint16_t num_queues;
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
} virtio_blk_config_t;

/* ============================================================================
 * Virtio Block Request Header
 * ============================================================================ */

typedef struct PACKED {
    uint32_t type;              /* VIRTIO_BLK_T_* */
    uint32_t reserved;
    uint64_t sector;            /* Starting sector (for read/write) */
} virtio_blk_req_hdr_t;

/* ============================================================================
 * Block Device Backend
 * ============================================================================ */

typedef struct blk_backend {
    /* Backend type */
    enum {
        BLK_BACKEND_MEMORY,     /* RAM disk */
        BLK_BACKEND_FILE,       /* File-backed */
        BLK_BACKEND_RAW         /* Raw device */
    } type;
    
    /* Disk properties */
    uint64_t size;              /* Size in bytes */
    uint32_t sector_size;       /* Sector size (usually 512) */
    bool readonly;
    char id[20];                /* Disk ID */
    
    /* Backend-specific data */
    void *data;
    
    /* Operations */
    int (*read)(struct blk_backend *be, uint64_t offset, 
                void *buf, size_t len);
    int (*write)(struct blk_backend *be, uint64_t offset,
                 const void *buf, size_t len);
    int (*flush)(struct blk_backend *be);
} blk_backend_t;

/* ============================================================================
 * Virtio Block Device
 * ============================================================================ */

typedef struct virtio_blk {
    virtio_device_t dev;
    virtio_blk_config_t config;
    blk_backend_t *backend;
} virtio_blk_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * virtio_blk_create - Create a virtio block device
 * @backend: Block backend to use
 * 
 * Returns virtio_blk device or NULL on failure
 */
virtio_blk_t *virtio_blk_create(blk_backend_t *backend);

/**
 * virtio_blk_destroy - Destroy a virtio block device
 * @blk: Device to destroy
 */
void virtio_blk_destroy(virtio_blk_t *blk);

/**
 * blk_backend_create_memory - Create RAM disk backend
 * @size: Size in bytes
 */
blk_backend_t *blk_backend_create_memory(uint64_t size);

/**
 * blk_backend_destroy - Destroy block backend
 * @be: Backend to destroy
 */
void blk_backend_destroy(blk_backend_t *be);

#endif /* _PUREVISOR_VIRTIO_BLK_H */
