/*
 * PureVisor - Cluster Node Header
 * 
 * Cluster node management and discovery
 */

#ifndef _PUREVISOR_CLUSTER_NODE_H
#define _PUREVISOR_CLUSTER_NODE_H

#include <lib/types.h>
#include <storage/block.h>

/* ============================================================================
 * Node Constants
 * ============================================================================ */

#define NODE_MAX_NAME           64
#define NODE_MAX_ADDRESS        64
#define NODE_MAX_TAGS           16
#define NODE_TAG_MAX_LEN        32

#define CLUSTER_MAX_NODES       64
#define CLUSTER_MAX_NAME        64

/* Node states */
#define NODE_STATE_UNKNOWN      0
#define NODE_STATE_JOINING      1
#define NODE_STATE_ONLINE       2
#define NODE_STATE_DEGRADED     3
#define NODE_STATE_OFFLINE      4
#define NODE_STATE_LEAVING      5
#define NODE_STATE_FAILED       6

/* Node roles */
#define NODE_ROLE_COMPUTE       BIT(0)
#define NODE_ROLE_STORAGE       BIT(1)
#define NODE_ROLE_NETWORK       BIT(2)
#define NODE_ROLE_MANAGEMENT    BIT(3)

/* Health check intervals */
#define HEALTH_CHECK_INTERVAL_MS    1000
#define HEALTH_TIMEOUT_MS           5000
#define HEARTBEAT_INTERVAL_MS       500

/* ============================================================================
 * Node Resources
 * ============================================================================ */

typedef struct node_cpu_info {
    uint32_t sockets;
    uint32_t cores_per_socket;
    uint32_t threads_per_core;
    uint32_t total_threads;
    uint64_t frequency_mhz;
    char model[64];
    bool vmx_supported;
    bool svm_supported;
} node_cpu_info_t;

typedef struct node_memory_info {
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint64_t used_bytes;
    uint64_t cached_bytes;
    uint64_t hugepages_total;
    uint64_t hugepages_free;
} node_memory_info_t;

typedef struct node_storage_info {
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint64_t used_bytes;
    uint32_t device_count;
    uint32_t pool_count;
    uint32_t volume_count;
} node_storage_info_t;

typedef struct node_network_info {
    uint32_t interface_count;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
} node_network_info_t;

typedef struct node_resources {
    node_cpu_info_t cpu;
    node_memory_info_t memory;
    node_storage_info_t storage;
    node_network_info_t network;
} node_resources_t;

/* ============================================================================
 * Node Health
 * ============================================================================ */

typedef struct node_health {
    uint32_t score;             /* 0-100 health score */
    uint64_t last_heartbeat;
    uint64_t last_health_check;
    uint32_t failed_checks;
    uint32_t consecutive_failures;
    
    /* Component health */
    bool cpu_healthy;
    bool memory_healthy;
    bool storage_healthy;
    bool network_healthy;
    
    /* Alerts */
    uint32_t active_alerts;
    char last_error[128];
} node_health_t;

/* ============================================================================
 * Cluster Node
 * ============================================================================ */

typedef struct cluster_node {
    /* Identity */
    uint32_t id;
    char name[NODE_MAX_NAME];
    char uuid[BLOCK_MAX_UUID];
    
    /* Network */
    char address[NODE_MAX_ADDRESS];
    uint16_t port;
    char management_address[NODE_MAX_ADDRESS];
    uint16_t management_port;
    
    /* State */
    uint32_t state;
    uint32_t roles;
    uint64_t joined_time;
    uint64_t uptime;
    
    /* Resources */
    node_resources_t resources;
    
    /* Health */
    node_health_t health;
    
    /* Tags for scheduling */
    char tags[NODE_MAX_TAGS][NODE_TAG_MAX_LEN];
    uint32_t tag_count;
    
    /* Workload */
    uint32_t vm_count;
    uint32_t container_count;
    
    /* Statistics */
    uint64_t total_vms_run;
    uint64_t total_migrations;
    
    /* Local flag */
    bool is_local;
    
    /* List linkage */
    struct cluster_node *next;
} cluster_node_t;

/* ============================================================================
 * Cluster
 * ============================================================================ */

typedef struct cluster {
    /* Identity */
    char name[CLUSTER_MAX_NAME];
    char uuid[BLOCK_MAX_UUID];
    
    /* Nodes */
    cluster_node_t *nodes;
    uint32_t node_count;
    uint32_t online_count;
    
    /* Local node */
    cluster_node_t *local_node;
    
    /* Leader (for management) */
    uint32_t leader_id;
    bool is_leader;
    
    /* Cluster state */
    uint32_t state;
    uint64_t formed_time;
    
    /* Quorum */
    uint32_t quorum_size;
    bool has_quorum;
    
    /* Statistics */
    uint64_t total_cpu_threads;
    uint64_t total_memory;
    uint64_t total_storage;
    
    /* Callbacks */
    void (*on_node_join)(struct cluster *c, cluster_node_t *node);
    void (*on_node_leave)(struct cluster *c, cluster_node_t *node);
    void (*on_leader_change)(struct cluster *c, uint32_t new_leader);
} cluster_t;

/* ============================================================================
 * Node API
 * ============================================================================ */

/**
 * node_create - Create a new node structure
 * @name: Node name
 * @address: Network address
 * @port: Service port
 */
cluster_node_t *node_create(const char *name, const char *address, uint16_t port);

/**
 * node_destroy - Destroy a node structure
 */
void node_destroy(cluster_node_t *node);

/**
 * node_set_state - Update node state
 */
void node_set_state(cluster_node_t *node, uint32_t state);

/**
 * node_add_role - Add a role to node
 */
void node_add_role(cluster_node_t *node, uint32_t role);

/**
 * node_remove_role - Remove a role from node
 */
void node_remove_role(cluster_node_t *node, uint32_t role);

/**
 * node_add_tag - Add a tag to node
 */
int node_add_tag(cluster_node_t *node, const char *tag);

/**
 * node_has_tag - Check if node has tag
 */
bool node_has_tag(cluster_node_t *node, const char *tag);

/**
 * node_update_resources - Update resource info
 */
void node_update_resources(cluster_node_t *node);

/**
 * node_health_check - Perform health check
 */
int node_health_check(cluster_node_t *node);

/**
 * node_get_state_string - Get state as string
 */
const char *node_get_state_string(uint32_t state);

/* ============================================================================
 * Cluster API
 * ============================================================================ */

/**
 * cluster_create - Create a new cluster
 * @name: Cluster name
 */
cluster_t *cluster_create(const char *name);

/**
 * cluster_destroy - Destroy a cluster
 */
void cluster_destroy(cluster_t *cluster);

/**
 * cluster_add_node - Add node to cluster
 */
int cluster_add_node(cluster_t *cluster, cluster_node_t *node);

/**
 * cluster_remove_node - Remove node from cluster
 */
int cluster_remove_node(cluster_t *cluster, cluster_node_t *node);

/**
 * cluster_find_node - Find node by ID
 */
cluster_node_t *cluster_find_node(cluster_t *cluster, uint32_t id);

/**
 * cluster_find_node_by_name - Find node by name
 */
cluster_node_t *cluster_find_node_by_name(cluster_t *cluster, const char *name);

/**
 * cluster_elect_leader - Trigger leader election
 */
int cluster_elect_leader(cluster_t *cluster);

/**
 * cluster_check_quorum - Check if quorum exists
 */
bool cluster_check_quorum(cluster_t *cluster);

/**
 * cluster_update_stats - Update cluster statistics
 */
void cluster_update_stats(cluster_t *cluster);

/**
 * cluster_tick - Process cluster maintenance
 */
void cluster_tick(cluster_t *cluster, uint64_t now_ms);

#endif /* _PUREVISOR_CLUSTER_NODE_H */
