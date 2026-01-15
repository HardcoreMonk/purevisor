/*
 * PureVisor - Management API Header
 * 
 * REST-like management interface
 */

#ifndef _PUREVISOR_MGMT_API_H
#define _PUREVISOR_MGMT_API_H

#include <lib/types.h>
#include <cluster/node.h>
#include <cluster/vm.h>
#include <cluster/scheduler.h>
#include <storage/pool.h>

/* ============================================================================
 * API Constants
 * ============================================================================ */

#define API_VERSION             "v1"
#define API_MAX_PATH            256
#define API_MAX_BODY            65536
#define API_MAX_RESPONSE        131072

/* HTTP Methods */
#define API_METHOD_GET          0
#define API_METHOD_POST         1
#define API_METHOD_PUT          2
#define API_METHOD_DELETE       3
#define API_METHOD_PATCH        4

/* HTTP Status Codes */
#define API_STATUS_OK           200
#define API_STATUS_CREATED      201
#define API_STATUS_ACCEPTED     202
#define API_STATUS_NO_CONTENT   204
#define API_STATUS_BAD_REQUEST  400
#define API_STATUS_UNAUTHORIZED 401
#define API_STATUS_FORBIDDEN    403
#define API_STATUS_NOT_FOUND    404
#define API_STATUS_CONFLICT     409
#define API_STATUS_ERROR        500

/* ============================================================================
 * API Request/Response
 * ============================================================================ */

typedef struct api_request {
    uint32_t method;
    char path[API_MAX_PATH];
    char *body;
    uint32_t body_len;
    
    /* Parsed path segments */
    char resource[64];
    char id[64];
    char action[64];
    
    /* Query parameters */
    char query[256];
} api_request_t;

typedef struct api_response {
    uint32_t status;
    char *body;
    uint32_t body_len;
    uint32_t body_capacity;
    
    /* Content type */
    char content_type[64];
} api_response_t;

/* ============================================================================
 * API Context
 * ============================================================================ */

typedef struct api_context {
    /* Resources */
    cluster_t *cluster;
    vm_manager_t *vm_manager;
    scheduler_t *scheduler;
    storage_pool_t *pool;
    
    /* Authentication */
    bool auth_required;
    char api_key[64];
    
    /* Statistics */
    uint64_t total_requests;
    uint64_t failed_requests;
} api_context_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * api_init - Initialize API context
 */
int api_init(api_context_t *ctx);

/**
 * api_handle_request - Process an API request
 */
int api_handle_request(api_context_t *ctx, api_request_t *req,
                        api_response_t *resp);

/**
 * api_response_init - Initialize response buffer
 */
int api_response_init(api_response_t *resp);

/**
 * api_response_free - Free response buffer
 */
void api_response_free(api_response_t *resp);

/**
 * api_response_json - Set JSON response body
 */
int api_response_json(api_response_t *resp, const char *json);

/**
 * api_response_error - Set error response
 */
int api_response_error(api_response_t *resp, uint32_t status, const char *message);

/* ============================================================================
 * JSON Helpers
 * ============================================================================ */

/**
 * json_node_info - Generate JSON for node info
 */
int json_node_info(cluster_node_t *node, char *buf, size_t size);

/**
 * json_cluster_info - Generate JSON for cluster info
 */
int json_cluster_info(cluster_t *cluster, char *buf, size_t size);

/**
 * json_vm_info - Generate JSON for VM info
 */
int json_vm_info(virtual_machine_t *vm, char *buf, size_t size);

/**
 * json_pool_info - Generate JSON for storage pool
 */
int json_pool_info(storage_pool_t *pool, char *buf, size_t size);

/**
 * json_volume_info - Generate JSON for volume
 */
int json_volume_info(storage_volume_t *vol, char *buf, size_t size);

#endif /* _PUREVISOR_MGMT_API_H */
