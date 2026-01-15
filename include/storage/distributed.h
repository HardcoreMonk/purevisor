/*
 * PureVisor - Distributed Storage Header
 * 
 * RAFT-based distributed storage consensus
 */

#ifndef _PUREVISOR_STORAGE_DISTRIBUTED_H
#define _PUREVISOR_STORAGE_DISTRIBUTED_H

#include <lib/types.h>
#include <storage/pool.h>

/* ============================================================================
 * RAFT Constants
 * ============================================================================ */

#define RAFT_MAX_NODES          16
#define RAFT_LOG_SIZE           1024
#define RAFT_HEARTBEAT_MS       150
#define RAFT_ELECTION_MIN_MS    300
#define RAFT_ELECTION_MAX_MS    500

/* RAFT states */
#define RAFT_FOLLOWER           0
#define RAFT_CANDIDATE          1
#define RAFT_LEADER             2

/* Log entry types */
#define RAFT_LOG_NOOP           0
#define RAFT_LOG_WRITE          1
#define RAFT_LOG_CONFIG         2
#define RAFT_LOG_SNAPSHOT       3

/* ============================================================================
 * RAFT Messages
 * ============================================================================ */

#define RAFT_MSG_VOTE_REQ       1
#define RAFT_MSG_VOTE_RESP      2
#define RAFT_MSG_APPEND_REQ     3
#define RAFT_MSG_APPEND_RESP    4
#define RAFT_MSG_SNAPSHOT       5

typedef struct PACKED {
    uint32_t type;
    uint32_t from_node;
    uint64_t term;
    uint32_t length;        /* Payload length */
} raft_msg_header_t;

typedef struct PACKED {
    raft_msg_header_t hdr;
    uint64_t last_log_index;
    uint64_t last_log_term;
} raft_vote_request_t;

typedef struct PACKED {
    raft_msg_header_t hdr;
    bool granted;
} raft_vote_response_t;

typedef struct PACKED {
    raft_msg_header_t hdr;
    uint64_t prev_log_index;
    uint64_t prev_log_term;
    uint64_t leader_commit;
    uint32_t entry_count;
    /* Followed by log entries */
} raft_append_request_t;

typedef struct PACKED {
    raft_msg_header_t hdr;
    bool success;
    uint64_t match_index;
} raft_append_response_t;

/* ============================================================================
 * RAFT Log Entry
 * ============================================================================ */

typedef struct raft_log_entry {
    uint64_t index;
    uint64_t term;
    uint32_t type;
    uint32_t data_len;
    uint8_t *data;
} raft_log_entry_t;

/* ============================================================================
 * RAFT Node
 * ============================================================================ */

typedef struct raft_node_info {
    uint32_t id;
    char address[64];
    uint16_t port;
    bool active;
    
    /* For leader: replication state */
    uint64_t next_index;
    uint64_t match_index;
    uint64_t last_contact;
} raft_node_info_t;

/* ============================================================================
 * RAFT Context
 * ============================================================================ */

typedef struct raft_context {
    /* Identity */
    uint32_t node_id;
    uint32_t state;
    
    /* Persistent state */
    uint64_t current_term;
    int32_t voted_for;
    
    /* Log */
    raft_log_entry_t *log;
    uint64_t log_size;
    uint64_t first_index;
    uint64_t last_index;
    
    /* Volatile state */
    uint64_t commit_index;
    uint64_t last_applied;
    
    /* Leader state */
    uint32_t leader_id;
    
    /* Cluster */
    raft_node_info_t nodes[RAFT_MAX_NODES];
    uint32_t node_count;
    uint32_t votes_received;
    
    /* Timing */
    uint64_t last_heartbeat;
    uint64_t election_timeout;
    
    /* Callbacks */
    int (*send_message)(struct raft_context *raft, uint32_t node_id,
                        void *msg, uint32_t len);
    int (*apply_entry)(struct raft_context *raft, raft_log_entry_t *entry);
    
    /* User data */
    void *priv;
} raft_context_t;

/* ============================================================================
 * Distributed Storage
 * ============================================================================ */

typedef struct dist_storage {
    /* Local storage */
    storage_pool_t *local_pool;
    
    /* RAFT consensus */
    raft_context_t raft;
    
    /* Cluster info */
    char cluster_name[64];
    char cluster_uuid[BLOCK_MAX_UUID];
    
    /* State */
    bool initialized;
    bool is_primary;
    
    /* Statistics */
    uint64_t replicated_writes;
    uint64_t consensus_ops;
} dist_storage_t;

/* ============================================================================
 * RAFT API
 * ============================================================================ */

/**
 * raft_init - Initialize RAFT context
 * @raft: Context to initialize
 * @node_id: This node's ID
 */
int raft_init(raft_context_t *raft, uint32_t node_id);

/**
 * raft_add_node - Add a node to cluster
 */
int raft_add_node(raft_context_t *raft, uint32_t id, 
                  const char *address, uint16_t port);

/**
 * raft_remove_node - Remove node from cluster
 */
int raft_remove_node(raft_context_t *raft, uint32_t id);

/**
 * raft_tick - Process a time tick
 */
void raft_tick(raft_context_t *raft, uint64_t now_ms);

/**
 * raft_recv_message - Process received message
 */
int raft_recv_message(raft_context_t *raft, void *msg, uint32_t len);

/**
 * raft_submit - Submit entry for replication
 */
int raft_submit(raft_context_t *raft, uint32_t type, 
                const void *data, uint32_t len);

/**
 * raft_is_leader - Check if this node is leader
 */
bool raft_is_leader(raft_context_t *raft);

/**
 * raft_get_leader - Get current leader ID
 */
uint32_t raft_get_leader(raft_context_t *raft);

/* ============================================================================
 * Distributed Storage API
 * ============================================================================ */

/**
 * dist_storage_init - Initialize distributed storage
 * @ds: Distributed storage context
 * @pool: Local storage pool
 * @node_id: This node's ID
 */
int dist_storage_init(dist_storage_t *ds, storage_pool_t *pool, uint32_t node_id);

/**
 * dist_storage_join - Join existing cluster
 */
int dist_storage_join(dist_storage_t *ds, const char *address, uint16_t port);

/**
 * dist_storage_write - Write with replication
 */
int dist_storage_write(dist_storage_t *ds, const char *volume,
                       uint64_t offset, const void *data, uint32_t len);

/**
 * dist_storage_read - Read (from local if available)
 */
int dist_storage_read(dist_storage_t *ds, const char *volume,
                      uint64_t offset, void *data, uint32_t len);

/**
 * dist_storage_get_status - Get cluster status
 */
int dist_storage_get_status(dist_storage_t *ds);

#endif /* _PUREVISOR_STORAGE_DISTRIBUTED_H */
