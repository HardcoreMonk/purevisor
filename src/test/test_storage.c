/*
 * PureVisor - Storage Test Suite
 * 
 * Unit tests for storage subsystem
 */

#include <lib/types.h>
#include <lib/string.h>
#include <test/framework.h>
#include <storage/block.h>
#include <storage/pool.h>
#include <storage/distributed.h>
#include <mm/heap.h>

/* ============================================================================
 * Block Layer Tests
 * ============================================================================ */

static test_result_t test_block_constants(void)
{
    TEST_ASSERT_EQ(BLOCK_SIZE_512, 512);
    TEST_ASSERT_EQ(BLOCK_SIZE_4K, 4096);
    
    return TEST_PASS;
}

static test_result_t test_block_request_create(void)
{
    block_request_t req = {0};
    
    req.op = BLOCK_OP_READ;
    req.offset = 0;
    req.length = 4096;
    
    TEST_ASSERT_EQ(req.op, BLOCK_OP_READ);
    TEST_ASSERT_EQ(req.offset, 0);
    TEST_ASSERT_EQ(req.length, 4096);
    
    return TEST_PASS;
}

static test_result_t test_block_ops(void)
{
    TEST_ASSERT_EQ(BLOCK_OP_READ, 0);
    TEST_ASSERT_EQ(BLOCK_OP_WRITE, 1);
    TEST_ASSERT_EQ(BLOCK_OP_FLUSH, 2);
    
    return TEST_PASS;
}

static test_case_t block_tests[] = {
    {"block_constants", test_block_constants},
    {"block_request_create", test_block_request_create},
    {"block_ops", test_block_ops},
};

static test_suite_t block_suite = {
    .name = "Block Layer",
    .setup = NULL,
    .teardown = NULL,
    .tests = block_tests,
    .test_count = sizeof(block_tests) / sizeof(block_tests[0]),
};

/* ============================================================================
 * Pool Tests
 * ============================================================================ */

static test_result_t test_pool_extent_size(void)
{
    /* Extent size should be 4MB */
    TEST_ASSERT_EQ(POOL_EXTENT_SIZE, 4 * 1024 * 1024);
    
    return TEST_PASS;
}

static test_result_t test_pool_replication_types(void)
{
    TEST_ASSERT_EQ(POOL_REPL_NONE, 0);
    TEST_ASSERT_EQ(POOL_REPL_MIRROR, 1);
    TEST_ASSERT_EQ(POOL_REPL_TRIPLE, 2);
    
    return TEST_PASS;
}

static test_result_t test_pool_states(void)
{
    TEST_ASSERT_EQ(POOL_STATE_OFFLINE, 0);
    TEST_ASSERT_EQ(POOL_STATE_DEGRADED, 1);
    TEST_ASSERT_EQ(POOL_STATE_ONLINE, 2);
    
    return TEST_PASS;
}

static test_case_t pool_tests[] = {
    {"pool_extent_size", test_pool_extent_size},
    {"pool_replication_types", test_pool_replication_types},
    {"pool_states", test_pool_states},
};

static test_suite_t pool_suite = {
    .name = "Storage Pool",
    .setup = NULL,
    .teardown = NULL,
    .tests = pool_tests,
    .test_count = sizeof(pool_tests) / sizeof(pool_tests[0]),
};

/* ============================================================================
 * RAFT Tests
 * ============================================================================ */

static test_result_t test_raft_states(void)
{
    TEST_ASSERT_EQ(RAFT_FOLLOWER, 0);
    TEST_ASSERT_EQ(RAFT_CANDIDATE, 1);
    TEST_ASSERT_EQ(RAFT_LEADER, 2);
    
    return TEST_PASS;
}

static test_result_t test_raft_log_types(void)
{
    TEST_ASSERT_EQ(RAFT_LOG_NOOP, 0);
    TEST_ASSERT_EQ(RAFT_LOG_WRITE, 1);
    TEST_ASSERT_EQ(RAFT_LOG_CONFIG, 2);
    
    return TEST_PASS;
}

static test_result_t test_raft_node_struct(void)
{
    raft_node_info_t node = {0};
    
    node.id = 1;
    strcpy(node.address, "192.168.1.1");
    node.port = 5000;
    node.next_index = 100;
    node.match_index = 99;
    
    TEST_ASSERT_EQ(node.id, 1);
    TEST_ASSERT_STR_EQ(node.address, "192.168.1.1");
    TEST_ASSERT_EQ(node.port, 5000);
    TEST_ASSERT_EQ(node.next_index, 100);
    TEST_ASSERT_EQ(node.match_index, 99);
    
    return TEST_PASS;
}

static test_result_t test_raft_constants(void)
{
    TEST_ASSERT_EQ(RAFT_MAX_NODES, 16);
    TEST_ASSERT_EQ(RAFT_LOG_SIZE, 1024);
    TEST_ASSERT_GT(RAFT_HEARTBEAT_MS, 0);
    
    return TEST_PASS;
}

static test_case_t raft_tests[] = {
    {"raft_states", test_raft_states},
    {"raft_log_types", test_raft_log_types},
    {"raft_node_struct", test_raft_node_struct},
    {"raft_constants", test_raft_constants},
};

static test_suite_t raft_suite = {
    .name = "RAFT Consensus",
    .setup = NULL,
    .teardown = NULL,
    .tests = raft_tests,
    .test_count = sizeof(raft_tests) / sizeof(raft_tests[0]),
};

/* ============================================================================
 * Suite Registration
 * ============================================================================ */

void test_storage_suite(void)
{
    test_register_suite(&block_suite);
    test_register_suite(&pool_suite);
    test_register_suite(&raft_suite);
}
