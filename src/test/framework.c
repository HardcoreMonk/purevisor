/*
 * PureVisor - Test Framework Implementation
 * 
 * Lightweight unit testing framework for bare-metal environment
 */

#include <lib/types.h>
#include <lib/string.h>
#include <test/framework.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * Test State
 * ============================================================================ */

#define MAX_SUITES 32

static test_suite_t *registered_suites[MAX_SUITES];
static uint32_t suite_count = 0;

static const char *current_test_name = NULL;
static uint64_t measure_start_time = 0;

/* TSC frequency approximation (assume ~2GHz for simplicity) */
#define TSC_TO_US_DIVISOR   2000

/* ============================================================================
 * Timing Utilities
 * ============================================================================ */

uint64_t test_get_time_us(void)
{
    return rdtsc() / TSC_TO_US_DIVISOR;
}

void test_measure_start(void)
{
    measure_start_time = rdtsc();
}

uint64_t test_measure_end(void)
{
    uint64_t end = rdtsc();
    return (end - measure_start_time) / TSC_TO_US_DIVISOR;
}

/* ============================================================================
 * Assertion Helpers
 * ============================================================================ */

void test_fail_at(const char *file, int line, const char *cond)
{
    kprintf("    [FAIL] %s:%d: assertion failed: %s\n", file, line, cond);
}

void test_fail_eq(const char *file, int line, const char *a_str, const char *b_str,
                  uint64_t a, uint64_t b)
{
    kprintf("    [FAIL] %s:%d: expected %s == %s, got %llu != %llu\n",
            file, line, a_str, b_str, a, b);
}

void test_fail_ne(const char *file, int line, const char *a_str, const char *b_str,
                  uint64_t val)
{
    kprintf("    [FAIL] %s:%d: expected %s != %s, both are %llu\n",
            file, line, a_str, b_str, val);
}

void test_fail_cmp(const char *file, int line, const char *a_str, const char *op,
                   const char *b_str, uint64_t a, uint64_t b)
{
    kprintf("    [FAIL] %s:%d: expected %s %s %s, got %llu vs %llu\n",
            file, line, a_str, op, b_str, a, b);
}

void test_fail_null(const char *file, int line, const char *ptr_str, bool expected_not_null)
{
    if (expected_not_null) {
        kprintf("    [FAIL] %s:%d: expected %s != NULL\n", file, line, ptr_str);
    } else {
        kprintf("    [FAIL] %s:%d: expected %s == NULL\n", file, line, ptr_str);
    }
}

void test_fail_str(const char *file, int line, const char *a_str, const char *b_str,
                   const char *a, const char *b)
{
    kprintf("    [FAIL] %s:%d: expected %s == %s\n", file, line, a_str, b_str);
    kprintf("           got: \"%s\" vs \"%s\"\n", a, b);
}

void test_fail_mem(const char *file, int line, const char *a_str, const char *b_str,
                   size_t len)
{
    kprintf("    [FAIL] %s:%d: memory mismatch between %s and %s (len=%zu)\n",
            file, line, a_str, b_str, len);
}

void test_skip_reason(const char *msg)
{
    kprintf("    [SKIP] %s\n", msg);
}

/* ============================================================================
 * Test Framework API
 * ============================================================================ */

void test_init(void)
{
    suite_count = 0;
    memset(registered_suites, 0, sizeof(registered_suites));
    kprintf("\n========================================\n");
    kprintf("  PureVisor Test Framework v1.0\n");
    kprintf("========================================\n\n");
}

void test_register_suite(test_suite_t *suite)
{
    if (suite_count < MAX_SUITES) {
        registered_suites[suite_count++] = suite;
    }
}

test_suite_stats_t test_run_suite(test_suite_t *suite)
{
    test_suite_stats_t stats = {0};
    stats.name = suite->name;
    
    kprintf("[Suite] %s\n", suite->name);
    kprintf("----------------------------------------\n");
    
    uint64_t suite_start = test_get_time_us();
    
    for (uint32_t i = 0; i < suite->test_count; i++) {
        test_case_t *tc = &suite->tests[i];
        current_test_name = tc->name;
        
        /* Run setup if provided */
        if (suite->setup) {
            suite->setup();
        }
        
        /* Run test */
        uint64_t test_start = test_get_time_us();
        test_result_t result = tc->func();
        uint64_t test_duration = test_get_time_us() - test_start;
        
        /* Run teardown if provided */
        if (suite->teardown) {
            suite->teardown();
        }
        
        /* Record result */
        stats.total++;
        
        switch (result) {
            case TEST_PASS:
                stats.passed++;
                kprintf("  [PASS] %s (%llu us)\n", tc->name, test_duration);
                break;
            case TEST_FAIL:
                stats.failed++;
                kprintf("  [FAIL] %s (%llu us)\n", tc->name, test_duration);
                break;
            case TEST_SKIP:
                stats.skipped++;
                /* Skip message already printed by macro */
                break;
            case TEST_ERROR:
                stats.errors++;
                kprintf("  [ERROR] %s (%llu us)\n", tc->name, test_duration);
                break;
        }
    }
    
    stats.total_duration_us = test_get_time_us() - suite_start;
    
    kprintf("----------------------------------------\n");
    kprintf("Suite: %u passed, %u failed, %u skipped, %u errors (%llu us)\n\n",
            stats.passed, stats.failed, stats.skipped, stats.errors,
            stats.total_duration_us);
    
    return stats;
}

test_summary_t test_run_all(void)
{
    test_summary_t summary = {0};
    
    uint64_t start = test_get_time_us();
    
    for (uint32_t i = 0; i < suite_count; i++) {
        test_suite_stats_t stats = test_run_suite(registered_suites[i]);
        
        summary.suites_run++;
        summary.total_tests += stats.total;
        summary.total_passed += stats.passed;
        summary.total_failed += stats.failed;
        summary.total_skipped += stats.skipped;
        summary.total_errors += stats.errors;
    }
    
    summary.total_duration_us = test_get_time_us() - start;
    
    return summary;
}

void test_print_summary(test_summary_t *summary)
{
    kprintf("========================================\n");
    kprintf("  TEST SUMMARY\n");
    kprintf("========================================\n");
    kprintf("  Suites:   %u\n", summary->suites_run);
    kprintf("  Tests:    %u\n", summary->total_tests);
    kprintf("  Passed:   %u\n", summary->total_passed);
    kprintf("  Failed:   %u\n", summary->total_failed);
    kprintf("  Skipped:  %u\n", summary->total_skipped);
    kprintf("  Errors:   %u\n", summary->total_errors);
    kprintf("  Duration: %llu us\n", summary->total_duration_us);
    kprintf("========================================\n");
    
    if (summary->total_failed == 0 && summary->total_errors == 0) {
        kprintf("  RESULT: ALL TESTS PASSED!\n");
    } else {
        kprintf("  RESULT: SOME TESTS FAILED!\n");
    }
    kprintf("========================================\n\n");
}
