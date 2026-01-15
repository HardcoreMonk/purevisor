/*
 * PureVisor - Integration Tests and Test Runner
 * 
 * End-to-end integration tests and main test runner
 */

#include <lib/types.h>
#include <lib/string.h>
#include <test/framework.h>
#include <test/benchmark.h>
#include <test/tests.h>
#include <kernel/console.h>
#include <mm/pmm.h>
#include <mm/heap.h>

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

static test_result_t test_memory_stress(void)
{
    /* Stress test: allocate and free many times */
    for (int iter = 0; iter < 100; iter++) {
        void *ptrs[16];
        
        /* Allocate */
        for (int i = 0; i < 16; i++) {
            ptrs[i] = kmalloc(64 + i * 32, GFP_KERNEL);
            if (!ptrs[i]) {
                /* Clean up and fail */
                for (int j = 0; j < i; j++) kfree(ptrs[j]);
                TEST_ASSERT_NOT_NULL(ptrs[i]);
            }
        }
        
        /* Free */
        for (int i = 0; i < 16; i++) {
            kfree(ptrs[i]);
        }
    }
    
    return TEST_PASS;
}

static test_result_t test_memory_fragmentation(void)
{
    void *ptrs[32];
    
    /* Allocate all */
    for (int i = 0; i < 32; i++) {
        ptrs[i] = kmalloc(128, GFP_KERNEL);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }
    
    /* Free every other one */
    for (int i = 0; i < 32; i += 2) {
        kfree(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    /* Allocate again (should reuse freed slots) */
    for (int i = 0; i < 32; i += 2) {
        ptrs[i] = kmalloc(128, GFP_KERNEL);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }
    
    /* Clean up */
    for (int i = 0; i < 32; i++) {
        kfree(ptrs[i]);
    }
    
    return TEST_PASS;
}

static test_result_t test_string_operations(void)
{
    char buf[256];
    
    /* Test strcpy */
    strcpy(buf, "Hello");
    TEST_ASSERT_STR_EQ(buf, "Hello");
    
    /* Test strcat */
    strcat(buf, " World");
    TEST_ASSERT_STR_EQ(buf, "Hello World");
    
    /* Test strlen */
    TEST_ASSERT_EQ(strlen(buf), 11);
    
    /* Test strcmp */
    TEST_ASSERT_EQ(strcmp("abc", "abc"), 0);
    TEST_ASSERT_LT(strcmp("abc", "abd"), 0);
    TEST_ASSERT_GT(strcmp("abd", "abc"), 0);
    
    /* Test memset */
    memset(buf, 'A', 10);
    buf[10] = '\0';
    TEST_ASSERT_STR_EQ(buf, "AAAAAAAAAA");
    
    /* Test memcpy */
    memcpy(buf, "Test123", 8);
    TEST_ASSERT_STR_EQ(buf, "Test123");
    
    return TEST_PASS;
}

static test_result_t test_data_integrity(void)
{
    /* Allocate buffer and fill with pattern */
    uint8_t *buf = kmalloc(4096, GFP_KERNEL);
    TEST_ASSERT_NOT_NULL(buf);
    
    /* Write pattern */
    for (int i = 0; i < 4096; i++) {
        buf[i] = (uint8_t)(i & 0xFF);
    }
    
    /* Verify pattern */
    for (int i = 0; i < 4096; i++) {
        TEST_ASSERT_EQ(buf[i], (uint8_t)(i & 0xFF));
    }
    
    kfree(buf);
    return TEST_PASS;
}

static test_case_t integration_tests[] = {
    {"memory_stress", test_memory_stress},
    {"memory_fragmentation", test_memory_fragmentation},
    {"string_operations", test_string_operations},
    {"data_integrity", test_data_integrity},
};

static test_suite_t integration_suite = {
    .name = "Integration Tests",
    .setup = NULL,
    .teardown = NULL,
    .tests = integration_tests,
    .test_count = sizeof(integration_tests) / sizeof(integration_tests[0]),
};

void test_integration_suite(void)
{
    test_register_suite(&integration_suite);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int run_all_tests(void)
{
    /* Initialize test framework */
    test_init();
    
    /* Register all test suites */
    test_pmm_suite();
    test_heap_suite();
    test_paging_suite();
    test_vmx_suite();
    test_storage_suite();
    test_cluster_suite();
    test_integration_suite();
    
    /* Run all tests */
    test_summary_t summary = test_run_all();
    
    /* Print summary */
    test_print_summary(&summary);
    
    /* Run benchmarks */
    bench_all();
    
    /* Return 0 if all passed */
    return (summary.total_failed == 0 && summary.total_errors == 0) ? 0 : 1;
}
