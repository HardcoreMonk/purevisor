/*
 * PureVisor - Test Suite Header
 * 
 * All test suite declarations
 */

#ifndef _PUREVISOR_TEST_TESTS_H
#define _PUREVISOR_TEST_TESTS_H

#include <test/framework.h>
#include <test/benchmark.h>

/* ============================================================================
 * Test Suite Declarations
 * ============================================================================ */

/* Memory Tests */
void test_pmm_suite(void);
void test_heap_suite(void);
void test_paging_suite(void);

/* VMX Tests */
void test_vmx_suite(void);

/* Storage Tests */
void test_storage_suite(void);

/* Cluster Tests */
void test_cluster_suite(void);

/* Integration Tests */
void test_integration_suite(void);

/* ============================================================================
 * Test Runner
 * ============================================================================ */

/**
 * run_all_tests - Run all test suites
 * 
 * Returns: 0 if all tests pass, non-zero otherwise
 */
int run_all_tests(void);

#endif /* _PUREVISOR_TEST_TESTS_H */
