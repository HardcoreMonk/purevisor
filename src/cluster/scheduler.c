/*
 * PureVisor - Scheduler Implementation
 * 
 * VM placement and resource scheduling
 */

#include <lib/types.h>
#include <lib/string.h>
#include <cluster/scheduler.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Scheduler Initialization
 * ============================================================================ */

int scheduler_init(scheduler_t *sched, cluster_t *cluster, vm_manager_t *mgr)
{
    if (!sched || !cluster || !mgr) return -1;
    
    memset(sched, 0, sizeof(*sched));
    
    sched->cluster = cluster;
    sched->vm_manager = mgr;
    sched->default_policy = SCHED_POLICY_SPREAD;
    sched->enable_overcommit = true;
    sched->cpu_overcommit_ratio = 200;      /* 2:1 */
    sched->memory_overcommit_ratio = 150;   /* 1.5:1 */
    
    pr_info("Scheduler: Initialized (policy=SPREAD, overcommit=enabled)");
    
    return 0;
}

/* ============================================================================
 * Node Filtering
 * ============================================================================ */

static bool node_is_feasible(scheduler_t *sched, cluster_node_t *node,
                             sched_request_t *req, char *reason)
{
    /* Check node state */
    if (node->state != NODE_STATE_ONLINE) {
        snprintf(reason, 64, "Node not online");
        return false;
    }
    
    /* Check health */
    if (node->health.score < 50) {
        snprintf(reason, 64, "Node unhealthy (score=%u)", node->health.score);
        return false;
    }
    
    /* Check forbidden nodes */
    for (uint32_t i = 0; i < req->forbidden_count; i++) {
        if (strcmp(node->name, req->forbidden_nodes[i]) == 0) {
            snprintf(reason, 64, "Node forbidden");
            return false;
        }
    }
    
    /* Check required tags */
    for (uint32_t i = 0; i < req->required_tag_count; i++) {
        if (!node_has_tag(node, req->required_tags[i])) {
            snprintf(reason, 64, "Missing tag: %s", req->required_tags[i]);
            return false;
        }
    }
    
    /* Check CPU capacity */
    uint64_t available_vcpus = node->resources.cpu.total_threads;
    if (sched->enable_overcommit) {
        available_vcpus = (available_vcpus * sched->cpu_overcommit_ratio) / 100;
    }
    /* Subtract existing VMs (simplified) */
    available_vcpus -= node->vm_count * 2;  /* Assume avg 2 vCPUs per VM */
    
    if (req->vcpus > available_vcpus) {
        snprintf(reason, 64, "Insufficient CPU");
        return false;
    }
    
    /* Check memory capacity */
    uint64_t available_mem = node->resources.memory.free_bytes;
    if (sched->enable_overcommit) {
        available_mem = (node->resources.memory.total_bytes * 
                         sched->memory_overcommit_ratio) / 100;
        available_mem -= node->resources.memory.used_bytes;
    }
    
    if (req->memory > available_mem) {
        snprintf(reason, 64, "Insufficient memory");
        return false;
    }
    
    return true;
}

/* ============================================================================
 * Node Scoring
 * ============================================================================ */

int scheduler_score_node(scheduler_t *sched, cluster_node_t *node,
                          sched_request_t *req, node_score_t *score)
{
    if (!sched || !node || !req || !score) return -1;
    
    memset(score, 0, sizeof(*score));
    score->node = node;
    
    /* Check feasibility first */
    score->feasible = node_is_feasible(sched, node, req, score->infeasible_reason);
    if (!score->feasible) {
        score->total_score = 0;
        return 0;
    }
    
    /* CPU score: prefer nodes with more free CPU */
    uint64_t total_cpu = node->resources.cpu.total_threads;
    uint64_t used_cpu = node->vm_count * 2;  /* Simplified */
    if (total_cpu > 0) {
        score->cpu_score = ((total_cpu - used_cpu) * 100) / total_cpu;
    }
    
    /* Memory score: prefer nodes with more free memory */
    uint64_t total_mem = node->resources.memory.total_bytes;
    uint64_t free_mem = node->resources.memory.free_bytes;
    if (total_mem > 0) {
        score->memory_score = (free_mem * 100) / total_mem;
    }
    
    /* Storage score */
    uint64_t total_storage = node->resources.storage.total_bytes;
    uint64_t free_storage = node->resources.storage.free_bytes;
    if (total_storage > 0) {
        score->storage_score = (free_storage * 100) / total_storage;
    } else {
        score->storage_score = 100;  /* No storage requirement */
    }
    
    /* Network score (based on health) */
    score->network_score = node->health.network_healthy ? 100 : 0;
    
    /* Affinity score */
    score->affinity_score = 50;  /* Neutral by default */
    
    /* Check VM affinity */
    for (uint32_t i = 0; i < req->affinity_count; i++) {
        virtual_machine_t *aff_vm = virt_vm_find(sched->vm_manager, 
                                            req->affinity_vm_ids[i]);
        if (aff_vm && aff_vm->host_node == node) {
            score->affinity_score += 25;
        }
    }
    
    /* Check VM anti-affinity */
    for (uint32_t i = 0; i < req->anti_affinity_count; i++) {
        virtual_machine_t *anti_vm = virt_vm_find(sched->vm_manager,
                                              req->anti_affinity_vm_ids[i]);
        if (anti_vm && anti_vm->host_node == node) {
            if (score->affinity_score >= 50) {
                score->affinity_score -= 50;
            } else {
                score->affinity_score = 0;
            }
        }
    }
    
    /* Cap affinity score */
    if (score->affinity_score > 100) score->affinity_score = 100;
    
    /* Calculate total weighted score */
    score->total_score = 
        (score->cpu_score * WEIGHT_CPU +
         score->memory_score * WEIGHT_MEMORY +
         score->storage_score * WEIGHT_STORAGE +
         score->network_score * WEIGHT_NETWORK) / 100;
    
    /* Adjust for policy */
    if (req->policy == SCHED_POLICY_PACK) {
        /* Prefer fuller nodes */
        score->total_score = 100 - score->total_score;
    }
    
    return 0;
}

/* ============================================================================
 * Scheduling
 * ============================================================================ */

int scheduler_schedule(scheduler_t *sched, sched_request_t *req,
                        sched_result_t *result)
{
    if (!sched || !req || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    cluster_t *cluster = sched->cluster;
    if (!cluster || cluster->node_count == 0) {
        result->success = false;
        strncpy(result->reason, "No nodes in cluster", 127);
        sched->failed_placements++;
        return -1;
    }
    
    /* Score all nodes */
    node_score_t *scores = kmalloc(cluster->node_count * sizeof(node_score_t),
                                    GFP_KERNEL | GFP_ZERO);
    if (!scores) {
        result->success = false;
        strncpy(result->reason, "Out of memory", 127);
        return -1;
    }
    
    uint32_t score_count = 0;
    cluster_node_t *node = cluster->nodes;
    
    while (node) {
        scheduler_score_node(sched, node, req, &scores[score_count]);
        score_count++;
        node = node->next;
    }
    
    /* Find best feasible node */
    node_score_t *best = NULL;
    for (uint32_t i = 0; i < score_count; i++) {
        if (scores[i].feasible) {
            if (!best || scores[i].total_score > best->total_score) {
                best = &scores[i];
            }
        }
    }
    
    if (best) {
        result->success = true;
        result->selected_node = best->node;
        result->score = best->total_score;
        snprintf(result->reason, 127, "Scheduled on %s (score=%u)",
                 best->node->name, best->total_score);
        
        /* Find alternatives */
        for (uint32_t i = 0; i < score_count && result->alternative_count < 3; i++) {
            if (scores[i].feasible && &scores[i] != best) {
                result->alternatives[result->alternative_count] = scores[i].node;
                result->alternative_scores[result->alternative_count] = 
                    scores[i].total_score;
                result->alternative_count++;
            }
        }
        
        sched->total_placements++;
        
        pr_info("Scheduler: Placed VM '%s' on node '%s' (score=%u)",
                req->vm ? req->vm->config.name : "unknown",
                best->node->name, best->total_score);
    } else {
        result->success = false;
        strncpy(result->reason, "No feasible node found", 127);
        sched->failed_placements++;
        
        pr_warn("Scheduler: Failed to place VM - no feasible nodes");
    }
    
    kfree(scores);
    return result->success ? 0 : -1;
}

/* ============================================================================
 * Rebalancing
 * ============================================================================ */

void scheduler_get_node_utilization(scheduler_t *sched, cluster_node_t *node,
                                     uint32_t *cpu_pct, uint32_t *mem_pct)
{
    (void)sched;
    
    if (!node) {
        *cpu_pct = 0;
        *mem_pct = 0;
        return;
    }
    
    /* CPU utilization (simplified) */
    uint64_t total_cpu = node->resources.cpu.total_threads;
    uint64_t used_cpu = node->vm_count * 2;
    *cpu_pct = total_cpu > 0 ? (used_cpu * 100) / total_cpu : 0;
    if (*cpu_pct > 100) *cpu_pct = 100;
    
    /* Memory utilization */
    uint64_t total_mem = node->resources.memory.total_bytes;
    uint64_t used_mem = node->resources.memory.used_bytes;
    *mem_pct = total_mem > 0 ? (used_mem * 100) / total_mem : 0;
}

int scheduler_rebalance(scheduler_t *sched)
{
    if (!sched || !sched->cluster) return -1;
    
    cluster_t *cluster = sched->cluster;
    
    /* Find imbalanced nodes */
    uint32_t avg_vms = 0;
    uint32_t online_nodes = 0;
    
    cluster_node_t *node = cluster->nodes;
    while (node) {
        if (node->state == NODE_STATE_ONLINE) {
            avg_vms += node->vm_count;
            online_nodes++;
        }
        node = node->next;
    }
    
    if (online_nodes == 0) return 0;
    avg_vms /= online_nodes;
    
    /* Find overloaded nodes and underloaded nodes */
    cluster_node_t *overloaded = NULL;
    cluster_node_t *underloaded = NULL;
    uint32_t max_vms = 0;
    uint32_t min_vms = 0xFFFFFFFF;
    
    node = cluster->nodes;
    while (node) {
        if (node->state == NODE_STATE_ONLINE) {
            if (node->vm_count > max_vms) {
                max_vms = node->vm_count;
                overloaded = node;
            }
            if (node->vm_count < min_vms) {
                min_vms = node->vm_count;
                underloaded = node;
            }
        }
        node = node->next;
    }
    
    /* Migrate if imbalance is significant */
    if (overloaded && underloaded && max_vms > min_vms + 2) {
        pr_info("Scheduler: Rebalancing from '%s' (%u VMs) to '%s' (%u VMs)",
                overloaded->name, max_vms, underloaded->name, min_vms);
        
        /* Find a VM to migrate */
        virtual_machine_t *vm = sched->vm_manager->vms;
        while (vm) {
            if (vm->host_node == overloaded && virt_vm_can_migrate(vm)) {
                virt_vm_migrate(sched->vm_manager, vm, underloaded);
                sched->migrations_triggered++;
                break;
            }
            vm = vm->next;
        }
    }
    
    return 0;
}

int scheduler_evacuate_node(scheduler_t *sched, cluster_node_t *node)
{
    if (!sched || !node) return -1;
    
    pr_info("Scheduler: Evacuating node '%s'", node->name);
    
    /* Find all VMs on this node and migrate them */
    virtual_machine_t *vm = sched->vm_manager->vms;
    while (vm) {
        if (vm->host_node == node) {
            /* Find another node */
            sched_request_t req = {0};
            req.vm = vm;
            req.vcpus = vm->config.vcpus;
            req.memory = vm->config.memory;
            req.forbidden_count = 1;
            strncpy(req.forbidden_nodes[0], node->name, NODE_MAX_NAME - 1);
            
            sched_result_t result;
            if (scheduler_schedule(sched, &req, &result) == 0) {
                virt_vm_migrate(sched->vm_manager, vm, result.selected_node);
                sched->migrations_triggered++;
            }
        }
        vm = vm->next;
    }
    
    return 0;
}
