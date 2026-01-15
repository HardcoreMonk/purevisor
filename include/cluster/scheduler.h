/*
 * PureVisor - Scheduler Header
 * 
 * VM placement and resource scheduling
 */

#ifndef _PUREVISOR_CLUSTER_SCHEDULER_H
#define _PUREVISOR_CLUSTER_SCHEDULER_H

#include <lib/types.h>
#include <cluster/node.h>
#include <cluster/vm.h>

/* ============================================================================
 * Scheduler Constants
 * ============================================================================ */

/* Scheduling policies */
#define SCHED_POLICY_SPREAD         0   /* Spread VMs across nodes */
#define SCHED_POLICY_PACK           1   /* Pack VMs onto fewest nodes */
#define SCHED_POLICY_RANDOM         2   /* Random placement */
#define SCHED_POLICY_AFFINITY       3   /* Follow affinity rules */

/* Scheduling priorities */
#define SCHED_PRIORITY_LOW          0
#define SCHED_PRIORITY_NORMAL       1
#define SCHED_PRIORITY_HIGH         2
#define SCHED_PRIORITY_CRITICAL     3

/* Resource weights (for scoring) */
#define WEIGHT_CPU                  40
#define WEIGHT_MEMORY               40
#define WEIGHT_STORAGE              10
#define WEIGHT_NETWORK              10

/* ============================================================================
 * Scheduling Request
 * ============================================================================ */

typedef struct sched_request {
    /* VM to schedule */
    virtual_machine_t *vm;
    
    /* Requirements */
    uint32_t vcpus;
    uint64_t memory;
    uint64_t storage;
    
    /* Preferences */
    uint32_t policy;
    uint32_t priority;
    
    /* Constraints */
    char required_tags[NODE_MAX_TAGS][NODE_TAG_MAX_LEN];
    uint32_t required_tag_count;
    
    char forbidden_nodes[CLUSTER_MAX_NODES][NODE_MAX_NAME];
    uint32_t forbidden_count;
    
    /* Affinity/Anti-affinity */
    uint32_t affinity_vm_ids[16];
    uint32_t affinity_count;
    
    uint32_t anti_affinity_vm_ids[16];
    uint32_t anti_affinity_count;
} sched_request_t;

/* ============================================================================
 * Scheduling Result
 * ============================================================================ */

typedef struct sched_result {
    bool success;
    cluster_node_t *selected_node;
    uint32_t score;
    char reason[128];
    
    /* Alternative nodes */
    cluster_node_t *alternatives[3];
    uint32_t alternative_scores[3];
    uint32_t alternative_count;
} sched_result_t;

/* ============================================================================
 * Node Score
 * ============================================================================ */

typedef struct node_score {
    cluster_node_t *node;
    uint32_t total_score;
    
    /* Component scores */
    uint32_t cpu_score;
    uint32_t memory_score;
    uint32_t storage_score;
    uint32_t network_score;
    uint32_t affinity_score;
    
    /* Feasibility */
    bool feasible;
    char infeasible_reason[64];
} node_score_t;

/* ============================================================================
 * Scheduler
 * ============================================================================ */

typedef struct scheduler {
    cluster_t *cluster;
    vm_manager_t *vm_manager;
    
    /* Configuration */
    uint32_t default_policy;
    bool enable_overcommit;
    uint32_t cpu_overcommit_ratio;      /* e.g., 200 = 2:1 */
    uint32_t memory_overcommit_ratio;
    
    /* Statistics */
    uint64_t total_placements;
    uint64_t failed_placements;
    uint64_t migrations_triggered;
} scheduler_t;

/* ============================================================================
 * Scheduler API
 * ============================================================================ */

/**
 * scheduler_init - Initialize scheduler
 */
int scheduler_init(scheduler_t *sched, cluster_t *cluster, vm_manager_t *mgr);

/**
 * scheduler_schedule - Schedule a VM placement
 * @sched: Scheduler
 * @req: Scheduling request
 * @result: Output result
 */
int scheduler_schedule(scheduler_t *sched, sched_request_t *req,
                        sched_result_t *result);

/**
 * scheduler_score_node - Calculate node score for a request
 */
int scheduler_score_node(scheduler_t *sched, cluster_node_t *node,
                          sched_request_t *req, node_score_t *score);

/**
 * scheduler_rebalance - Rebalance VMs across cluster
 */
int scheduler_rebalance(scheduler_t *sched);

/**
 * scheduler_evacuate_node - Migrate all VMs off a node
 */
int scheduler_evacuate_node(scheduler_t *sched, cluster_node_t *node);

/**
 * scheduler_get_node_utilization - Get node resource utilization
 */
void scheduler_get_node_utilization(scheduler_t *sched, cluster_node_t *node,
                                     uint32_t *cpu_pct, uint32_t *mem_pct);

#endif /* _PUREVISOR_CLUSTER_SCHEDULER_H */
