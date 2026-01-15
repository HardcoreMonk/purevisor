/*
 * PureVisor - Management API Implementation
 * 
 * REST-like management interface
 */

#include <lib/types.h>
#include <lib/string.h>
#include <mgmt/api.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Response Helpers
 * ============================================================================ */

int api_response_init(api_response_t *resp)
{
    if (!resp) return -1;
    
    resp->body = kmalloc(API_MAX_RESPONSE, GFP_KERNEL);
    if (!resp->body) return -1;
    
    resp->body[0] = '\0';
    resp->body_len = 0;
    resp->body_capacity = API_MAX_RESPONSE;
    resp->status = API_STATUS_OK;
    strcpy(resp->content_type, "application/json");
    
    return 0;
}

void api_response_free(api_response_t *resp)
{
    if (resp && resp->body) {
        kfree(resp->body);
        resp->body = NULL;
    }
}

int api_response_json(api_response_t *resp, const char *json)
{
    if (!resp || !json) return -1;
    
    size_t len = strlen(json);
    if (len >= resp->body_capacity) {
        return -1;
    }
    
    strcpy(resp->body, json);
    resp->body_len = len;
    resp->status = API_STATUS_OK;
    
    return 0;
}

int api_response_error(api_response_t *resp, uint32_t status, const char *message)
{
    if (!resp) return -1;
    
    resp->status = status;
    snprintf(resp->body, resp->body_capacity,
             "{\"error\":{\"status\":%u,\"message\":\"%s\"}}",
             status, message ? message : "Unknown error");
    resp->body_len = strlen(resp->body);
    
    return 0;
}

/* ============================================================================
 * JSON Generation
 * ============================================================================ */

int json_node_info(cluster_node_t *node, char *buf, size_t size)
{
    if (!node || !buf) return -1;
    
    return snprintf(buf, size,
        "{"
        "\"id\":%u,"
        "\"name\":\"%s\","
        "\"uuid\":\"%s\","
        "\"address\":\"%s\","
        "\"port\":%u,"
        "\"state\":\"%s\","
        "\"roles\":%u,"
        "\"health\":{\"score\":%u,\"cpu\":%s,\"memory\":%s},"
        "\"resources\":{"
        "\"cpu\":{\"threads\":%u,\"vmx\":%s},"
        "\"memory\":{\"total\":%llu,\"free\":%llu},"
        "\"storage\":{\"total\":%llu,\"free\":%llu}"
        "},"
        "\"workload\":{\"vms\":%u}"
        "}",
        node->id,
        node->name,
        node->uuid,
        node->address,
        node->port,
        node_get_state_string(node->state),
        node->roles,
        node->health.score,
        node->health.cpu_healthy ? "true" : "false",
        node->health.memory_healthy ? "true" : "false",
        node->resources.cpu.total_threads,
        node->resources.cpu.vmx_supported ? "true" : "false",
        node->resources.memory.total_bytes,
        node->resources.memory.free_bytes,
        node->resources.storage.total_bytes,
        node->resources.storage.free_bytes,
        node->vm_count);
}

int json_cluster_info(cluster_t *cluster, char *buf, size_t size)
{
    if (!cluster || !buf) return -1;
    
    return snprintf(buf, size,
        "{"
        "\"name\":\"%s\","
        "\"uuid\":\"%s\","
        "\"nodes\":{\"total\":%u,\"online\":%u},"
        "\"leader\":%u,"
        "\"quorum\":{\"size\":%u,\"has_quorum\":%s},"
        "\"resources\":{"
        "\"cpu_threads\":%llu,"
        "\"memory\":%llu,"
        "\"storage\":%llu"
        "}"
        "}",
        cluster->name,
        cluster->uuid,
        cluster->node_count,
        cluster->online_count,
        cluster->leader_id,
        cluster->quorum_size,
        cluster->has_quorum ? "true" : "false",
        cluster->total_cpu_threads,
        cluster->total_memory,
        cluster->total_storage);
}

int json_vm_info(virtual_machine_t *vm, char *buf, size_t size)
{
    if (!vm || !buf) return -1;
    
    return snprintf(buf, size,
        "{"
        "\"id\":%u,"
        "\"uuid\":\"%s\","
        "\"name\":\"%s\","
        "\"state\":\"%s\","
        "\"config\":{"
        "\"vcpus\":%u,"
        "\"memory\":%llu,"
        "\"disks\":%u,"
        "\"nics\":%u"
        "},"
        "\"host_node\":%u,"
        "\"stats\":{"
        "\"cpu_time\":%llu,"
        "\"vmexit_count\":%llu"
        "}"
        "}",
        vm->id,
        vm->uuid,
        vm->config.name,
        vm_get_state_string(vm->state),
        vm->config.vcpus,
        vm->config.memory,
        vm->config.disk_count,
        vm->config.nic_count,
        vm->host_node_id,
        vm->stats.cpu_time_ns,
        vm->stats.vmexit_count);
}

int json_pool_info(storage_pool_t *pool, char *buf, size_t size)
{
    if (!pool || !buf) return -1;
    
    return snprintf(buf, size,
        "{"
        "\"name\":\"%s\","
        "\"uuid\":\"%s\","
        "\"state\":%u,"
        "\"capacity\":{"
        "\"total\":%llu,"
        "\"free\":%llu,"
        "\"used\":%llu"
        "},"
        "\"devices\":%u,"
        "\"volumes\":%u,"
        "\"extents\":{\"total\":%u,\"free\":%u}"
        "}",
        pool->name,
        pool->uuid,
        pool->state,
        pool->total_size,
        pool->free_size,
        pool->used_size,
        pool->device_count,
        pool->volume_count,
        pool->total_extents,
        pool->free_extents);
}

int json_volume_info(storage_volume_t *vol, char *buf, size_t size)
{
    if (!vol || !buf) return -1;
    
    return snprintf(buf, size,
        "{"
        "\"name\":\"%s\","
        "\"uuid\":\"%s\","
        "\"size\":%llu,"
        "\"allocated\":%llu,"
        "\"thin\":%s,"
        "\"online\":%s,"
        "\"replication\":%u"
        "}",
        vol->name,
        vol->uuid,
        vol->size,
        vol->allocated,
        vol->thin_provisioned ? "true" : "false",
        vol->online ? "true" : "false",
        vol->replication);
}

/* ============================================================================
 * Request Parsing
 * ============================================================================ */

static void parse_path(api_request_t *req)
{
    /* Parse path like /api/v1/vms/123/start */
    char *p = req->path;
    
    /* Skip /api/v1/ prefix */
    if (strncmp(p, "/api/v1/", 8) == 0) {
        p += 8;
    } else if (*p == '/') {
        p++;
    }
    
    /* Get resource */
    char *slash = strchr(p, '/');
    if (slash) {
        size_t len = slash - p;
        if (len > 63) len = 63;
        strncpy(req->resource, p, len);
        req->resource[len] = '\0';
        p = slash + 1;
    } else {
        strncpy(req->resource, p, 63);
        return;
    }
    
    /* Get ID */
    slash = strchr(p, '/');
    if (slash) {
        size_t len = slash - p;
        if (len > 63) len = 63;
        strncpy(req->id, p, len);
        req->id[len] = '\0';
        p = slash + 1;
    } else {
        strncpy(req->id, p, 63);
        return;
    }
    
    /* Get action */
    strncpy(req->action, p, 63);
}

/* ============================================================================
 * Request Handlers
 * ============================================================================ */

static int handle_cluster(api_context_t *ctx, api_request_t *req,
                          api_response_t *resp)
{
    if (req->method == API_METHOD_GET) {
        if (ctx->cluster) {
            json_cluster_info(ctx->cluster, resp->body, resp->body_capacity);
            resp->body_len = strlen(resp->body);
            return 0;
        }
        return api_response_error(resp, API_STATUS_NOT_FOUND, "Cluster not found");
    }
    
    return api_response_error(resp, API_STATUS_BAD_REQUEST, "Invalid method");
}

static int handle_nodes(api_context_t *ctx, api_request_t *req,
                        api_response_t *resp)
{
    if (!ctx->cluster) {
        return api_response_error(resp, API_STATUS_NOT_FOUND, "Cluster not found");
    }
    
    if (req->method == API_METHOD_GET) {
        if (req->id[0] == '\0') {
            /* List all nodes */
            char *p = resp->body;
            size_t remain = resp->body_capacity;
            
            int n = snprintf(p, remain, "{\"nodes\":[");
            p += n; remain -= n;
            
            bool first = true;
            cluster_node_t *node = ctx->cluster->nodes;
            while (node && remain > 256) {
                if (!first) {
                    *p++ = ',';
                    remain--;
                }
                n = json_node_info(node, p, remain);
                if (n > 0) {
                    p += n;
                    remain -= n;
                }
                first = false;
                node = node->next;
            }
            
            snprintf(p, remain, "]}");
            resp->body_len = strlen(resp->body);
            return 0;
        } else {
            /* Get specific node */
            uint32_t id = 0;
            for (const char *s = req->id; *s; s++) {
                id = id * 10 + (*s - '0');
            }
            
            cluster_node_t *node = cluster_find_node(ctx->cluster, id);
            if (node) {
                json_node_info(node, resp->body, resp->body_capacity);
                resp->body_len = strlen(resp->body);
                return 0;
            }
            return api_response_error(resp, API_STATUS_NOT_FOUND, "Node not found");
        }
    }
    
    return api_response_error(resp, API_STATUS_BAD_REQUEST, "Invalid method");
}

static int handle_vms(api_context_t *ctx, api_request_t *req,
                      api_response_t *resp)
{
    if (!ctx->vm_manager) {
        return api_response_error(resp, API_STATUS_ERROR, "VM manager not available");
    }
    
    if (req->method == API_METHOD_GET) {
        if (req->id[0] == '\0') {
            /* List all VMs */
            char *p = resp->body;
            size_t remain = resp->body_capacity;
            
            int n = snprintf(p, remain, "{\"vms\":[");
            p += n; remain -= n;
            
            bool first = true;
            virtual_machine_t *vm = ctx->vm_manager->vms;
            while (vm && remain > 256) {
                if (!first) {
                    *p++ = ',';
                    remain--;
                }
                n = json_vm_info(vm, p, remain);
                if (n > 0) {
                    p += n;
                    remain -= n;
                }
                first = false;
                vm = vm->next;
            }
            
            snprintf(p, remain, "]}");
            resp->body_len = strlen(resp->body);
            return 0;
        }
    }
    
    if (req->method == API_METHOD_POST && req->action[0] != '\0') {
        /* VM actions: start, stop, pause, resume */
        uint32_t id = 0;
        for (const char *s = req->id; *s >= '0' && *s <= '9'; s++) {
            id = id * 10 + (*s - '0');
        }
        
        virtual_machine_t *vm = virt_vm_find(ctx->vm_manager, id);
        if (!vm) {
            return api_response_error(resp, API_STATUS_NOT_FOUND, "VM not found");
        }
        
        int ret = -1;
        if (strcmp(req->action, "start") == 0) {
            ret = virt_vm_start(ctx->vm_manager, vm);
        } else if (strcmp(req->action, "stop") == 0) {
            ret = virt_vm_stop(ctx->vm_manager, vm);
        } else if (strcmp(req->action, "pause") == 0) {
            ret = virt_vm_pause(ctx->vm_manager, vm);
        } else if (strcmp(req->action, "resume") == 0) {
            ret = virt_vm_resume(ctx->vm_manager, vm);
        }
        
        if (ret == 0) {
            resp->status = API_STATUS_ACCEPTED;
            json_vm_info(vm, resp->body, resp->body_capacity);
            resp->body_len = strlen(resp->body);
            return 0;
        }
        return api_response_error(resp, API_STATUS_CONFLICT, vm->error_msg);
    }
    
    return api_response_error(resp, API_STATUS_BAD_REQUEST, "Invalid request");
}

static int handle_pools(api_context_t *ctx, api_request_t *req,
                        api_response_t *resp)
{
    if (req->method == API_METHOD_GET && ctx->pool) {
        json_pool_info(ctx->pool, resp->body, resp->body_capacity);
        resp->body_len = strlen(resp->body);
        return 0;
    }
    
    return api_response_error(resp, API_STATUS_NOT_FOUND, "Pool not found");
}

/* ============================================================================
 * Main API Handler
 * ============================================================================ */

int api_init(api_context_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->auth_required = false;
    
    pr_info("API: Initialized management API");
    return 0;
}

int api_handle_request(api_context_t *ctx, api_request_t *req,
                        api_response_t *resp)
{
    if (!ctx || !req || !resp) return -1;
    
    ctx->total_requests++;
    
    /* Parse the path */
    parse_path(req);
    
    /* Route to handler */
    int ret;
    if (strcmp(req->resource, "cluster") == 0) {
        ret = handle_cluster(ctx, req, resp);
    } else if (strcmp(req->resource, "nodes") == 0) {
        ret = handle_nodes(ctx, req, resp);
    } else if (strcmp(req->resource, "vms") == 0) {
        ret = handle_vms(ctx, req, resp);
    } else if (strcmp(req->resource, "pools") == 0) {
        ret = handle_pools(ctx, req, resp);
    } else {
        ret = api_response_error(resp, API_STATUS_NOT_FOUND, "Resource not found");
    }
    
    if (ret != 0) {
        ctx->failed_requests++;
    }
    
    return ret;
}
