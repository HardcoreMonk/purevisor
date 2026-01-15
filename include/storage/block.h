/*
 * PureVisor - Block Storage Header
 * 
 * Core block storage abstractions and interfaces
 */

#ifndef _PUREVISOR_STORAGE_BLOCK_H
#define _PUREVISOR_STORAGE_BLOCK_H

#include <lib/types.h>

/* ============================================================================
 * Block Storage Constants
 * ============================================================================ */

#define BLOCK_SIZE_512          512
#define BLOCK_SIZE_4K           4096
#define BLOCK_DEFAULT_SIZE      BLOCK_SIZE_4K

#define BLOCK_MAX_NAME          64
#define BLOCK_MAX_UUID          37  /* UUID string + null */

#define BLOCK_OP_READ           0
#define BLOCK_OP_WRITE          1
#define BLOCK_OP_FLUSH          2
#define BLOCK_OP_DISCARD        3
#define BLOCK_OP_WRITE_ZEROES   4

/* Block request flags */
#define BLOCK_REQ_FUA           BIT(0)  /* Force Unit Access */
#define BLOCK_REQ_PREFLUSH      BIT(1)  /* Pre-flush */
#define BLOCK_REQ_SYNC          BIT(2)  /* Synchronous */

/* ============================================================================
 * Block Request
 * ============================================================================ */

typedef void (*block_completion_t)(void *ctx, int status);

typedef struct block_request {
    /* Operation */
    uint8_t op;
    uint8_t flags;
    uint16_t reserved;
    
    /* Location */
    uint64_t offset;        /* Byte offset */
    uint32_t length;        /* Length in bytes */
    
    /* Data */
    void *buffer;
    
    /* Completion */
    block_completion_t completion;
    void *completion_ctx;
    int status;
    
    /* Queue linkage */
    struct block_request *next;
} block_request_t;

/* ============================================================================
 * Block Device
 * ============================================================================ */

typedef struct block_device block_device_t;

typedef struct block_ops {
    int (*open)(block_device_t *dev);
    void (*close)(block_device_t *dev);
    int (*submit)(block_device_t *dev, block_request_t *req);
    int (*flush)(block_device_t *dev);
    int (*get_info)(block_device_t *dev);
} block_ops_t;

struct block_device {
    /* Identity */
    char name[BLOCK_MAX_NAME];
    char uuid[BLOCK_MAX_UUID];
    uint32_t id;
    
    /* Geometry */
    uint64_t size;          /* Total size in bytes */
    uint32_t block_size;    /* Block size */
    uint64_t num_blocks;    /* Number of blocks */
    
    /* Properties */
    bool readonly;
    bool removable;
    bool online;
    
    /* Operations */
    const block_ops_t *ops;
    
    /* Statistics */
    uint64_t read_ops;
    uint64_t write_ops;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t errors;
    
    /* Request queue */
    block_request_t *queue_head;
    block_request_t *queue_tail;
    uint32_t queue_depth;
    uint32_t max_queue_depth;
    
    /* Private data */
    void *priv;
    
    /* List linkage */
    struct block_device *next;
};

/* ============================================================================
 * Block Layer API
 * ============================================================================ */

/**
 * block_init - Initialize block subsystem
 */
int block_init(void);

/**
 * block_register - Register a block device
 */
int block_register(block_device_t *dev);

/**
 * block_unregister - Unregister a block device
 */
void block_unregister(block_device_t *dev);

/**
 * block_find_by_name - Find device by name
 */
block_device_t *block_find_by_name(const char *name);

/**
 * block_find_by_uuid - Find device by UUID
 */
block_device_t *block_find_by_uuid(const char *uuid);

/**
 * block_read - Synchronous read
 */
int block_read(block_device_t *dev, uint64_t offset, void *buf, uint32_t len);

/**
 * block_write - Synchronous write
 */
int block_write(block_device_t *dev, uint64_t offset, const void *buf, uint32_t len);

/**
 * block_flush - Flush device
 */
int block_flush(block_device_t *dev);

/**
 * block_submit_async - Submit async request
 */
int block_submit_async(block_device_t *dev, block_request_t *req);

/**
 * block_alloc_request - Allocate a request
 */
block_request_t *block_alloc_request(void);

/**
 * block_free_request - Free a request
 */
void block_free_request(block_request_t *req);

/**
 * block_generate_uuid - Generate a UUID string
 */
void block_generate_uuid(char *uuid);

#endif /* _PUREVISOR_STORAGE_BLOCK_H */
