/*
 * PureVisor - Cluster Node Implementation
 * 
 * Cluster node management and discovery
 */

#include <lib/types.h>
#include <lib/string.h>
#include <cluster/node.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <kernel/smp.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * State Strings
 * ============================================================================ */

static const char *state_strings[] = {
    [NODE_STATE_UNKNOWN]  = "UNKNOWN",
    [NODE_STATE_JOINING]  = "JOINING",
    [NODE_STATE_ONLINE]   = "ONLINE",
    [NODE_STATE_DEGRADED] = "DEGRADED",
    [NODE_STATE_OFFLINE]  = "OFFLINE",
    [NODE_STATE_LEAVING]  = "LEAVING",
    [NODE_STATE_FAILED]   = "FAILED",
};

const char *node_get_state_string(uint32_t state)
{
    if (state <= NODE_STATE_FAILED) {
        return state_strings[state];
    }
    return "INVALID";
}

/* ============================================================================
 * Node Management
 * ============================================================================ */

cluster_node_t *node_create(const char *name, const char *address, uint16_t port)
{
    cluster_node_t *node = kmalloc(sizeof(cluster_node_t), GFP_KERNEL | GFP_ZERO);
    if (!node) return NULL;
    
    static uint32_t next_node_id = 1;
    node->id = next_node_id++;
    
    strncpy(node->name, name, NODE_MAX_NAME - 1);
    strncpy(node->address, address, NODE_MAX_ADDRESS - 1);
    node->port = port;
    
    /* Generate UUID */
    block_generate_uuid(node->uuid);
    
    node->state = NODE_STATE_UNKNOWN;
    node->health.score = 100;
    
    pr_info("Node: Created '%s' (ID=%u)", name, node->id);
    
    return node;
}

void node_destroy(cluster_node_t *node)
{
    if (!node) return;
    pr_info("Node: Destroyed '%s'", node->name);
    kfree(node);
}

void node_set_state(cluster_node_t *node, uint32_t state)
{
    if (!node) return;
    
    uint32_t old_state = node->state;
    node->state = state;
    
    pr_info("Node: '%s' state changed: %s -> %s",
            node->name,
            node_get_state_string(old_state),
            node_get_state_string(state));
}

void node_add_role(cluster_node_t *node, uint32_t role)
{
    if (!node) return;
    node->roles |= role;
}

void node_remove_role(cluster_node_t *node, uint32_t role)
{
    if (!node) return;
    node->roles &= ~role;
}

int node_add_tag(cluster_node_t *node, const char *tag)
{
    if (!node || !tag) return -1;
    if (node->tag_count >= NODE_MAX_TAGS) return -1;
    
    strncpy(node->tags[node->tag_count], tag, NODE_TAG_MAX_LEN - 1);
    node->tag_count++;
    return 0;
}

bool node_has_tag(cluster_node_t *node, const char *tag)
{
    if (!node || !tag) return false;
    
    for (uint32_t i = 0; i < node->tag_count; i++) {
        if (strcmp(node->tags[i], tag) == 0) {
            return true;
        }
    }
    return false;
}

void node_update_resources(cluster_node_t *node)
{
    if (!node) return;
    
    /* Update CPU info */
    node->resources.cpu.total_threads = smp_get_cpu_count();
    node->resources.cpu.vmx_supported = cpu_features.vmx_supported;
    node->resources.cpu.svm_supported = cpu_features.svm_supported;
    strncpy(node->resources.cpu.model, cpu_features.brand, 63);
    
    /* Memory info would come from PMM */
    extern uint64_t pmm_get_total_pages(void);
    extern uint64_t pmm_get_free_pages(void);
    node->resources.memory.total_bytes = pmm_get_total_pages() * 4096;
    node->resources.memory.free_bytes = pmm_get_free_pages() * 4096;
    node->resources.memory.used_bytes = 
        node->resources.memory.total_bytes - node->resources.memory.free_bytes;
}

int node_health_check(cluster_node_t *node)
{
    if (!node) return -1;
    
    node->health.last_health_check = rdtsc();
    
    /* Check CPU */
    node->health.cpu_healthy = true;
    
    /* Check memory */
    node->resources.memory.free_bytes > 0 ?
        (node->health.memory_healthy = true) :
        (node->health.memory_healthy = false);
    
    /* Check storage */
    node->health.storage_healthy = true;
    
    /* Check network */
    node->health.network_healthy = true;
    
    /* Calculate health score */
    uint32_t score = 100;
    if (!node->health.cpu_healthy) score -= 25;
    if (!node->health.memory_healthy) score -= 25;
    if (!node->health.storage_healthy) score -= 25;
    if (!node->health.network_healthy) score -= 25;
    
    node->health.score = score;
    
    if (score < 50) {
        node->health.failed_checks++;
        node->health.consecutive_failures++;
    } else {
        node->health.consecutive_failures = 0;
    }
    
    return 0;
}

/* ============================================================================
 * Cluster Management
 * ============================================================================ */

cluster_t *cluster_create(const char *name)
{
    cluster_t *cluster = kmalloc(sizeof(cluster_t), GFP_KERNEL | GFP_ZERO);
    if (!cluster) return NULL;
    
    strncpy(cluster->name, name, CLUSTER_MAX_NAME - 1);
    block_generate_uuid(cluster->uuid);
    
    cluster->quorum_size = 1;  /* Single node by default */
    cluster->formed_time = rdtsc();
    
    pr_info("Cluster: Created '%s' (%s)", name, cluster->uuid);
    
    return cluster;
}

void cluster_destroy(cluster_t *cluster)
{
    if (!cluster) return;
    
    /* Destroy all nodes */
    cluster_node_t *node = cluster->nodes;
    while (node) {
        cluster_node_t *next = node->next;
        node_destroy(node);
        node = next;
    }
    
    pr_info("Cluster: Destroyed '%s'", cluster->name);
    kfree(cluster);
}

int cluster_add_node(cluster_t *cluster, cluster_node_t *node)
{
    if (!cluster || !node) return -1;
    if (cluster->node_count >= CLUSTER_MAX_NODES) return -1;
    
    node->state = NODE_STATE_JOINING;
    node->joined_time = rdtsc();
    
    /* Add to list */
    node->next = cluster->nodes;
    cluster->nodes = node;
    cluster->node_count++;
    
    /* Update quorum */
    cluster->quorum_size = (cluster->node_count / 2) + 1;
    
    node->state = NODE_STATE_ONLINE;
    cluster->online_count++;
    
    /* Update stats */
    cluster_update_stats(cluster);
    
    /* Callback */
    if (cluster->on_node_join) {
        cluster->on_node_join(cluster, node);
    }
    
    pr_info("Cluster: Node '%s' joined '%s' (%u nodes)",
            node->name, cluster->name, cluster->node_count);
    
    return 0;
}

int cluster_remove_node(cluster_t *cluster, cluster_node_t *node)
{
    if (!cluster || !node) return -1;
    
    node->state = NODE_STATE_LEAVING;
    
    /* Remove from list */
    cluster_node_t **pp = &cluster->nodes;
    while (*pp) {
        if (*pp == node) {
            *pp = node->next;
            cluster->node_count--;
            cluster->online_count--;
            break;
        }
        pp = &(*pp)->next;
    }
    
    /* Update quorum */
    if (cluster->node_count > 0) {
        cluster->quorum_size = (cluster->node_count / 2) + 1;
    } else {
        cluster->quorum_size = 1;
    }
    
    /* Callback */
    if (cluster->on_node_leave) {
        cluster->on_node_leave(cluster, node);
    }
    
    pr_info("Cluster: Node '%s' left '%s' (%u nodes)",
            node->name, cluster->name, cluster->node_count);
    
    /* Update stats */
    cluster_update_stats(cluster);
    
    return 0;
}

cluster_node_t *cluster_find_node(cluster_t *cluster, uint32_t id)
{
    if (!cluster) return NULL;
    
    cluster_node_t *node = cluster->nodes;
    while (node) {
        if (node->id == id) return node;
        node = node->next;
    }
    return NULL;
}

cluster_node_t *cluster_find_node_by_name(cluster_t *cluster, const char *name)
{
    if (!cluster || !name) return NULL;
    
    cluster_node_t *node = cluster->nodes;
    while (node) {
        if (strcmp(node->name, name) == 0) return node;
        node = node->next;
    }
    return NULL;
}

int cluster_elect_leader(cluster_t *cluster)
{
    if (!cluster || !cluster->nodes) return -1;
    
    /* Simple election: lowest ID online node becomes leader */
    cluster_node_t *leader = NULL;
    cluster_node_t *node = cluster->nodes;
    
    while (node) {
        if (node->state == NODE_STATE_ONLINE) {
            if (!leader || node->id < leader->id) {
                leader = node;
            }
        }
        node = node->next;
    }
    
    if (leader) {
        uint32_t old_leader = cluster->leader_id;
        cluster->leader_id = leader->id;
        cluster->is_leader = (cluster->local_node && 
                              cluster->local_node->id == leader->id);
        
        if (old_leader != leader->id) {
            pr_info("Cluster: New leader elected: %s (ID=%u)",
                    leader->name, leader->id);
            
            if (cluster->on_leader_change) {
                cluster->on_leader_change(cluster, leader->id);
            }
        }
        return 0;
    }
    
    return -1;
}

bool cluster_check_quorum(cluster_t *cluster)
{
    if (!cluster) return false;
    
    cluster->has_quorum = (cluster->online_count >= cluster->quorum_size);
    return cluster->has_quorum;
}

void cluster_update_stats(cluster_t *cluster)
{
    if (!cluster) return;
    
    cluster->total_cpu_threads = 0;
    cluster->total_memory = 0;
    cluster->total_storage = 0;
    cluster->online_count = 0;
    
    cluster_node_t *node = cluster->nodes;
    while (node) {
        if (node->state == NODE_STATE_ONLINE) {
            cluster->online_count++;
            cluster->total_cpu_threads += node->resources.cpu.total_threads;
            cluster->total_memory += node->resources.memory.total_bytes;
            cluster->total_storage += node->resources.storage.total_bytes;
        }
        node = node->next;
    }
}

void cluster_tick(cluster_t *cluster, uint64_t now_ms)
{
    if (!cluster) return;
    
    cluster_node_t *node = cluster->nodes;
    while (node) {
        /* Check for failed nodes */
        if (node->state == NODE_STATE_ONLINE && !node->is_local) {
            uint64_t elapsed = now_ms - node->health.last_heartbeat;
            if (elapsed > HEALTH_TIMEOUT_MS) {
                node_set_state(node, NODE_STATE_FAILED);
                cluster->online_count--;
                cluster_check_quorum(cluster);
                cluster_elect_leader(cluster);
            }
        }
        
        /* Update uptime */
        if (node->state == NODE_STATE_ONLINE) {
            node->uptime = now_ms - node->joined_time;
        }
        
        node = node->next;
    }
}
