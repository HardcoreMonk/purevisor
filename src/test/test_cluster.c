/*
 * PureVisor - Cluster Test Suite
 * 
 * Unit tests for cluster management
 */

#include <lib/types.h>
#include <lib/string.h>
#include <test/framework.h>
#include <cluster/node.h>
#include <cluster/vm.h>
#include <cluster/scheduler.h>
#include <mm/heap.h>

/* ============================================================================
 * Node Tests
 * ============================================================================ */

static test_result_t test_node_states(void)
{
    TEST_ASSERT_EQ(NODE_STATE_UNKNOWN, 0);
    TEST_ASSERT_EQ(NODE_STATE_JOINING, 1);
    TEST_ASSERT_EQ(NODE_STATE_ONLINE, 2);
    TEST_ASSERT_EQ(NODE_STATE_DEGRADED, 3);
    TEST_ASSERT_EQ(NODE_STATE_OFFLINE, 4);
    
    return TEST_PASS;
}

static test_result_t test_node_roles(void)
{
    TEST_ASSERT_EQ(NODE_ROLE_COMPUTE, BIT(0));
    TEST_ASSERT_EQ(NODE_ROLE_STORAGE, BIT(1));
    TEST_ASSERT_EQ(NODE_ROLE_NETWORK, BIT(2));
    TEST_ASSERT_EQ(NODE_ROLE_MANAGEMENT, BIT(3));
    
    return TEST_PASS;
}

static test_result_t test_node_struct_init(void)
{
    cluster_node_t node = {0};
    
    node.id = 1;
    strcpy(node.name, "node-01");
    node.state = NODE_STATE_ONLINE;
    node.roles = NODE_ROLE_COMPUTE | NODE_ROLE_STORAGE;
    
    TEST_ASSERT_EQ(node.id, 1);
    TEST_ASSERT_STR_EQ(node.name, "node-01");
    TEST_ASSERT_EQ(node.state, NODE_STATE_ONLINE);
    TEST_ASSERT(node.roles & NODE_ROLE_COMPUTE);
    TEST_ASSERT(node.roles & NODE_ROLE_STORAGE);
    
    return TEST_PASS;
}

static test_result_t test_node_constants(void)
{
    TEST_ASSERT_GT(NODE_MAX_NAME, 0);
    TEST_ASSERT_GT(CLUSTER_MAX_NODES, 0);
    TEST_ASSERT_GT(HEALTH_CHECK_INTERVAL_MS, 0);
    
    return TEST_PASS;
}

static test_case_t node_tests[] = {
    {"node_states", test_node_states},
    {"node_roles", test_node_roles},
    {"node_struct_init", test_node_struct_init},
    {"node_constants", test_node_constants},
};

static test_suite_t node_suite = {
    .name = "Cluster Nodes",
    .setup = NULL,
    .teardown = NULL,
    .tests = node_tests,
    .test_count = sizeof(node_tests) / sizeof(node_tests[0]),
};

/* ============================================================================
 * VM Tests
 * ============================================================================ */

static test_result_t test_vm_states(void)
{
    TEST_ASSERT_EQ(VM_STATE_CREATED, 0);
    TEST_ASSERT_EQ(VM_STATE_STARTING, 1);
    TEST_ASSERT_EQ(VM_STATE_RUNNING, 2);
    TEST_ASSERT_EQ(VM_STATE_PAUSED, 3);
    TEST_ASSERT_EQ(VM_STATE_STOPPING, 4);
    TEST_ASSERT_EQ(VM_STATE_STOPPED, 5);
    
    return TEST_PASS;
}

static test_result_t test_vm_constants(void)
{
    TEST_ASSERT_GT(VM_MAX_DISKS, 0);
    TEST_ASSERT_GT(VM_MAX_NICS, 0);
    
    return TEST_PASS;
}

static test_result_t test_vm_config_struct(void)
{
    vm_config_t config = {0};
    
    strcpy(config.name, "test-vm");
    config.vcpus = 4;
    config.memory = 4ULL * 1024 * 1024 * 1024;  /* 4GB */
    
    TEST_ASSERT_STR_EQ(config.name, "test-vm");
    TEST_ASSERT_EQ(config.vcpus, 4);
    TEST_ASSERT_EQ(config.memory, 4ULL * 1024 * 1024 * 1024);
    
    return TEST_PASS;
}

static test_case_t vm_tests[] = {
    {"vm_states", test_vm_states},
    {"vm_constants", test_vm_constants},
    {"vm_config_struct", test_vm_config_struct},
};

static test_suite_t vm_suite = {
    .name = "Virtual Machines",
    .setup = NULL,
    .teardown = NULL,
    .tests = vm_tests,
    .test_count = sizeof(vm_tests) / sizeof(vm_tests[0]),
};

/* ============================================================================
 * Scheduler Tests
 * ============================================================================ */

static test_result_t test_scheduler_policies(void)
{
    TEST_ASSERT_EQ(SCHED_POLICY_SPREAD, 0);
    TEST_ASSERT_EQ(SCHED_POLICY_PACK, 1);
    TEST_ASSERT_EQ(SCHED_POLICY_RANDOM, 2);
    
    return TEST_PASS;
}

static test_result_t test_scheduler_constants(void)
{
    /* Check priority constants */
    TEST_ASSERT_EQ(SCHED_PRIORITY_LOW, 0);
    TEST_ASSERT_EQ(SCHED_PRIORITY_NORMAL, 1);
    TEST_ASSERT_EQ(SCHED_PRIORITY_HIGH, 2);
    
    return TEST_PASS;
}

static test_case_t scheduler_tests[] = {
    {"scheduler_policies", test_scheduler_policies},
    {"scheduler_constants", test_scheduler_constants},
};

static test_suite_t scheduler_suite = {
    .name = "VM Scheduler",
    .setup = NULL,
    .teardown = NULL,
    .tests = scheduler_tests,
    .test_count = sizeof(scheduler_tests) / sizeof(scheduler_tests[0]),
};

/* ============================================================================
 * Suite Registration
 * ============================================================================ */

void test_cluster_suite(void)
{
    test_register_suite(&node_suite);
    test_register_suite(&vm_suite);
    test_register_suite(&scheduler_suite);
}
