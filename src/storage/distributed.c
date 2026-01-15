/*
 * PureVisor - RAFT Consensus Implementation
 * 
 * Distributed consensus for storage replication
 */

#include <lib/types.h>
#include <lib/string.h>
#include <storage/distributed.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_random_timeout(uint64_t min_ms, uint64_t max_ms)
{
    uint64_t tsc = rdtsc();
    return min_ms + (tsc % (max_ms - min_ms));
}

static raft_log_entry_t *get_log_entry(raft_context_t *raft, uint64_t index)
{
    if (index < raft->first_index || index > raft->last_index) {
        return NULL;
    }
    return &raft->log[index - raft->first_index];
}

static uint64_t get_last_log_term(raft_context_t *raft)
{
    if (raft->last_index == 0) return 0;
    raft_log_entry_t *entry = get_log_entry(raft, raft->last_index);
    return entry ? entry->term : 0;
}

static int append_log_entry(raft_context_t *raft, uint64_t term,
                            uint32_t type, const void *data, uint32_t len)
{
    if (raft->last_index - raft->first_index >= RAFT_LOG_SIZE - 1) {
        return -1;  /* Log full */
    }
    
    uint64_t idx = ++raft->last_index;
    raft_log_entry_t *entry = &raft->log[idx - raft->first_index];
    
    entry->index = idx;
    entry->term = term;
    entry->type = type;
    entry->data_len = len;
    
    if (len > 0 && data) {
        entry->data = kmalloc(len, GFP_KERNEL);
        if (!entry->data) return -1;
        memcpy(entry->data, data, len);
    } else {
        entry->data = NULL;
    }
    
    return 0;
}

/* ============================================================================
 * State Transitions
 * ============================================================================ */

static void become_follower(raft_context_t *raft, uint64_t term)
{
    raft->state = RAFT_FOLLOWER;
    raft->current_term = term;
    raft->voted_for = -1;
    raft->votes_received = 0;
    
    pr_info("RAFT[%u]: Became FOLLOWER (term %llu)", 
            raft->node_id, term);
}

static void become_candidate(raft_context_t *raft)
{
    raft->state = RAFT_CANDIDATE;
    raft->current_term++;
    raft->voted_for = raft->node_id;
    raft->votes_received = 1;  /* Vote for self */
    raft->election_timeout = get_random_timeout(RAFT_ELECTION_MIN_MS, 
                                                 RAFT_ELECTION_MAX_MS);
    
    pr_info("RAFT[%u]: Became CANDIDATE (term %llu)", 
            raft->node_id, raft->current_term);
    
    /* Send vote requests */
    raft_vote_request_t req;
    req.hdr.type = RAFT_MSG_VOTE_REQ;
    req.hdr.from_node = raft->node_id;
    req.hdr.term = raft->current_term;
    req.hdr.length = sizeof(req) - sizeof(raft_msg_header_t);
    req.last_log_index = raft->last_index;
    req.last_log_term = get_last_log_term(raft);
    
    for (uint32_t i = 0; i < raft->node_count; i++) {
        if (raft->nodes[i].id != raft->node_id && raft->nodes[i].active) {
            if (raft->send_message) {
                raft->send_message(raft, raft->nodes[i].id, &req, sizeof(req));
            }
        }
    }
}

static void become_leader(raft_context_t *raft)
{
    raft->state = RAFT_LEADER;
    raft->leader_id = raft->node_id;
    
    /* Initialize leader state */
    for (uint32_t i = 0; i < raft->node_count; i++) {
        raft->nodes[i].next_index = raft->last_index + 1;
        raft->nodes[i].match_index = 0;
    }
    
    /* Append no-op entry */
    append_log_entry(raft, raft->current_term, RAFT_LOG_NOOP, NULL, 0);
    
    pr_info("RAFT[%u]: Became LEADER (term %llu)", 
            raft->node_id, raft->current_term);
}

/* ============================================================================
 * Message Handlers
 * ============================================================================ */

static void handle_vote_request(raft_context_t *raft, raft_vote_request_t *req)
{
    raft_vote_response_t resp;
    resp.hdr.type = RAFT_MSG_VOTE_RESP;
    resp.hdr.from_node = raft->node_id;
    resp.hdr.term = raft->current_term;
    resp.hdr.length = sizeof(resp) - sizeof(raft_msg_header_t);
    resp.granted = false;
    
    /* Update term if needed */
    if (req->hdr.term > raft->current_term) {
        become_follower(raft, req->hdr.term);
    }
    
    /* Check if we can grant vote */
    if (req->hdr.term >= raft->current_term &&
        (raft->voted_for == -1 || raft->voted_for == (int32_t)req->hdr.from_node)) {
        
        /* Check log is up-to-date */
        uint64_t last_term = get_last_log_term(raft);
        if (req->last_log_term > last_term ||
            (req->last_log_term == last_term && 
             req->last_log_index >= raft->last_index)) {
            
            resp.granted = true;
            raft->voted_for = req->hdr.from_node;
            raft->last_heartbeat = rdtsc();  /* Reset election timeout */
        }
    }
    
    resp.hdr.term = raft->current_term;
    
    if (raft->send_message) {
        raft->send_message(raft, req->hdr.from_node, &resp, sizeof(resp));
    }
}

static void handle_vote_response(raft_context_t *raft, raft_vote_response_t *resp)
{
    if (resp->hdr.term > raft->current_term) {
        become_follower(raft, resp->hdr.term);
        return;
    }
    
    if (raft->state != RAFT_CANDIDATE || 
        resp->hdr.term != raft->current_term) {
        return;
    }
    
    if (resp->granted) {
        raft->votes_received++;
        
        /* Check for majority */
        uint32_t majority = (raft->node_count / 2) + 1;
        if (raft->votes_received >= majority) {
            become_leader(raft);
        }
    }
}

static void handle_append_request(raft_context_t *raft, raft_append_request_t *req)
{
    raft_append_response_t resp;
    resp.hdr.type = RAFT_MSG_APPEND_RESP;
    resp.hdr.from_node = raft->node_id;
    resp.hdr.term = raft->current_term;
    resp.hdr.length = sizeof(resp) - sizeof(raft_msg_header_t);
    resp.success = false;
    resp.match_index = 0;
    
    /* Update term if needed */
    if (req->hdr.term > raft->current_term) {
        become_follower(raft, req->hdr.term);
    }
    
    if (req->hdr.term < raft->current_term) {
        goto send_response;
    }
    
    raft->leader_id = req->hdr.from_node;
    raft->last_heartbeat = rdtsc();
    
    if (raft->state == RAFT_CANDIDATE) {
        become_follower(raft, req->hdr.term);
    }
    
    /* Check log consistency */
    if (req->prev_log_index > 0) {
        raft_log_entry_t *prev = get_log_entry(raft, req->prev_log_index);
        if (!prev || prev->term != req->prev_log_term) {
            goto send_response;
        }
    }
    
    /* Append entries (simplified - would need to parse entry data) */
    resp.success = true;
    resp.match_index = req->prev_log_index + req->entry_count;
    
    /* Update commit index */
    if (req->leader_commit > raft->commit_index) {
        raft->commit_index = req->leader_commit;
        if (raft->commit_index > raft->last_index) {
            raft->commit_index = raft->last_index;
        }
    }
    
send_response:
    resp.hdr.term = raft->current_term;
    if (raft->send_message) {
        raft->send_message(raft, req->hdr.from_node, &resp, sizeof(resp));
    }
}

static void handle_append_response(raft_context_t *raft, raft_append_response_t *resp)
{
    if (resp->hdr.term > raft->current_term) {
        become_follower(raft, resp->hdr.term);
        return;
    }
    
    if (raft->state != RAFT_LEADER) {
        return;
    }
    
    /* Find node */
    raft_node_info_t *node = NULL;
    for (uint32_t i = 0; i < raft->node_count; i++) {
        if (raft->nodes[i].id == resp->hdr.from_node) {
            node = &raft->nodes[i];
            break;
        }
    }
    
    if (!node) return;
    
    if (resp->success) {
        node->match_index = resp->match_index;
        node->next_index = resp->match_index + 1;
        
        /* Update commit index */
        for (uint64_t n = raft->commit_index + 1; n <= raft->last_index; n++) {
            uint32_t count = 1;  /* Leader */
            for (uint32_t i = 0; i < raft->node_count; i++) {
                if (raft->nodes[i].match_index >= n) {
                    count++;
                }
            }
            
            uint32_t majority = (raft->node_count / 2) + 1;
            raft_log_entry_t *entry = get_log_entry(raft, n);
            
            if (count >= majority && entry && entry->term == raft->current_term) {
                raft->commit_index = n;
            }
        }
    } else {
        /* Decrement next_index and retry */
        if (node->next_index > 1) {
            node->next_index--;
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int raft_init(raft_context_t *raft, uint32_t node_id)
{
    memset(raft, 0, sizeof(*raft));
    
    raft->node_id = node_id;
    raft->state = RAFT_FOLLOWER;
    raft->current_term = 0;
    raft->voted_for = -1;
    raft->leader_id = 0;
    
    raft->log = kmalloc(RAFT_LOG_SIZE * sizeof(raft_log_entry_t), 
                        GFP_KERNEL | GFP_ZERO);
    if (!raft->log) return -1;
    
    raft->log_size = RAFT_LOG_SIZE;
    raft->first_index = 0;
    raft->last_index = 0;
    raft->commit_index = 0;
    raft->last_applied = 0;
    
    raft->election_timeout = get_random_timeout(RAFT_ELECTION_MIN_MS, 
                                                 RAFT_ELECTION_MAX_MS);
    
    pr_info("RAFT[%u]: Initialized", node_id);
    
    return 0;
}

int raft_add_node(raft_context_t *raft, uint32_t id, 
                  const char *address, uint16_t port)
{
    if (raft->node_count >= RAFT_MAX_NODES) {
        return -1;
    }
    
    raft_node_info_t *node = &raft->nodes[raft->node_count++];
    node->id = id;
    strncpy(node->address, address, 63);
    node->port = port;
    node->active = true;
    node->next_index = raft->last_index + 1;
    node->match_index = 0;
    
    pr_info("RAFT[%u]: Added node %u (%s:%u)",
            raft->node_id, id, address, port);
    
    return 0;
}

int raft_remove_node(raft_context_t *raft, uint32_t id)
{
    for (uint32_t i = 0; i < raft->node_count; i++) {
        if (raft->nodes[i].id == id) {
            raft->nodes[i].active = false;
            return 0;
        }
    }
    return -1;
}

void raft_tick(raft_context_t *raft, uint64_t now_ms)
{
    /* Apply committed entries */
    while (raft->last_applied < raft->commit_index) {
        raft->last_applied++;
        raft_log_entry_t *entry = get_log_entry(raft, raft->last_applied);
        if (entry && raft->apply_entry) {
            raft->apply_entry(raft, entry);
        }
    }
    
    if (raft->state == RAFT_LEADER) {
        /* Send heartbeats */
        static uint64_t last_hb = 0;
        if (now_ms - last_hb >= RAFT_HEARTBEAT_MS) {
            last_hb = now_ms;
            
            raft_append_request_t req;
            req.hdr.type = RAFT_MSG_APPEND_REQ;
            req.hdr.from_node = raft->node_id;
            req.hdr.term = raft->current_term;
            req.hdr.length = sizeof(req) - sizeof(raft_msg_header_t);
            req.leader_commit = raft->commit_index;
            req.entry_count = 0;
            
            for (uint32_t i = 0; i < raft->node_count; i++) {
                if (raft->nodes[i].id != raft->node_id && raft->nodes[i].active) {
                    req.prev_log_index = raft->nodes[i].next_index - 1;
                    raft_log_entry_t *prev = get_log_entry(raft, req.prev_log_index);
                    req.prev_log_term = prev ? prev->term : 0;
                    
                    if (raft->send_message) {
                        raft->send_message(raft, raft->nodes[i].id, &req, sizeof(req));
                    }
                }
            }
        }
    } else {
        /* Check election timeout */
        if (now_ms - raft->last_heartbeat >= raft->election_timeout) {
            become_candidate(raft);
            raft->last_heartbeat = now_ms;
        }
    }
}

int raft_recv_message(raft_context_t *raft, void *msg, uint32_t len UNUSED)
{
    raft_msg_header_t *hdr = (raft_msg_header_t *)msg;
    
    switch (hdr->type) {
        case RAFT_MSG_VOTE_REQ:
            handle_vote_request(raft, (raft_vote_request_t *)msg);
            break;
        case RAFT_MSG_VOTE_RESP:
            handle_vote_response(raft, (raft_vote_response_t *)msg);
            break;
        case RAFT_MSG_APPEND_REQ:
            handle_append_request(raft, (raft_append_request_t *)msg);
            break;
        case RAFT_MSG_APPEND_RESP:
            handle_append_response(raft, (raft_append_response_t *)msg);
            break;
        default:
            return -1;
    }
    
    return 0;
}

int raft_submit(raft_context_t *raft, uint32_t type, 
                const void *data, uint32_t len)
{
    if (raft->state != RAFT_LEADER) {
        return -1;  /* Not leader */
    }
    
    return append_log_entry(raft, raft->current_term, type, data, len);
}

bool raft_is_leader(raft_context_t *raft)
{
    return raft->state == RAFT_LEADER;
}

uint32_t raft_get_leader(raft_context_t *raft)
{
    return raft->leader_id;
}

/* ============================================================================
 * Distributed Storage Implementation
 * ============================================================================ */

static int apply_storage_entry(raft_context_t *raft, raft_log_entry_t *entry)
{
    dist_storage_t *ds = (dist_storage_t *)raft->priv;
    
    if (entry->type == RAFT_LOG_WRITE && entry->data) {
        /* Apply write to local storage */
        /* Format: [volume_name(64)][offset(8)][len(4)][data] */
        char *vol_name = (char *)entry->data;
        uint64_t offset = *(uint64_t *)(entry->data + 64);
        uint32_t len = *(uint32_t *)(entry->data + 72);
        void *data = entry->data + 76;
        
        storage_volume_t *vol = NULL;
        storage_pool_t *pool = ds->local_pool;
        for (storage_volume_t *v = pool->volumes; v; v = v->next) {
            if (strcmp(v->name, vol_name) == 0) {
                vol = v;
                break;
            }
        }
        
        if (vol) {
            block_write(&vol->blkdev, offset, data, len);
        }
    }
    
    return 0;
}

int dist_storage_init(dist_storage_t *ds, storage_pool_t *pool, uint32_t node_id)
{
    memset(ds, 0, sizeof(*ds));
    
    ds->local_pool = pool;
    
    if (raft_init(&ds->raft, node_id) != 0) {
        return -1;
    }
    
    ds->raft.priv = ds;
    ds->raft.apply_entry = apply_storage_entry;
    
    block_generate_uuid(ds->cluster_uuid);
    strcpy(ds->cluster_name, "purevisor-cluster");
    
    ds->initialized = true;
    
    pr_info("DistStorage: Initialized node %u", node_id);
    
    return 0;
}

int dist_storage_join(dist_storage_t *ds, const char *address, uint16_t port)
{
    /* Add remote node */
    static uint32_t next_remote = 100;
    return raft_add_node(&ds->raft, next_remote++, address, port);
}

int dist_storage_write(dist_storage_t *ds, const char *volume,
                       uint64_t offset, const void *data, uint32_t len)
{
    if (!ds->initialized) return -1;
    
    if (!raft_is_leader(&ds->raft)) {
        return -1;  /* Redirect to leader */
    }
    
    /* Create log entry */
    uint32_t entry_len = 64 + 8 + 4 + len;
    uint8_t *entry_data = kmalloc(entry_len, GFP_KERNEL);
    if (!entry_data) return -1;
    
    memset(entry_data, 0, 64);
    strncpy((char *)entry_data, volume, 63);
    *(uint64_t *)(entry_data + 64) = offset;
    *(uint32_t *)(entry_data + 72) = len;
    memcpy(entry_data + 76, data, len);
    
    int ret = raft_submit(&ds->raft, RAFT_LOG_WRITE, entry_data, entry_len);
    kfree(entry_data);
    
    if (ret == 0) {
        ds->replicated_writes++;
    }
    
    return ret;
}

int dist_storage_read(dist_storage_t *ds, const char *volume,
                      uint64_t offset, void *data, uint32_t len)
{
    if (!ds->initialized) return -1;
    
    /* Read from local storage */
    storage_pool_t *pool = ds->local_pool;
    for (storage_volume_t *v = pool->volumes; v; v = v->next) {
        if (strcmp(v->name, volume) == 0) {
            return block_read(&v->blkdev, offset, data, len);
        }
    }
    
    return -1;
}

int dist_storage_get_status(dist_storage_t *ds)
{
    if (!ds->initialized) return -1;
    return ds->raft.state;
}
