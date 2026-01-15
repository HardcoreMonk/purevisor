/*
 * PureVisor - Test Framework Header
 * 
 * Lightweight unit testing framework for bare-metal environment
 */

#ifndef _PUREVISOR_TEST_FRAMEWORK_H
#define _PUREVISOR_TEST_FRAMEWORK_H

#include <lib/types.h>

/* ============================================================================
 * Test Result Types
 * ============================================================================ */

typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1,
    TEST_SKIP = 2,
    TEST_ERROR = 3
} test_result_t;

typedef struct {
    const char *name;
    const char *suite;
    test_result_t result;
    const char *message;
    uint64_t duration_us;  /* Execution time in microseconds */
} test_case_result_t;

/* ============================================================================
 * Test Suite Statistics
 * ============================================================================ */

typedef struct {
    const char *name;
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
    uint32_t errors;
    uint64_t total_duration_us;
} test_suite_stats_t;

typedef struct {
    uint32_t suites_run;
    uint32_t total_tests;
    uint32_t total_passed;
    uint32_t total_failed;
    uint32_t total_skipped;
    uint32_t total_errors;
    uint64_t total_duration_us;
} test_summary_t;

/* ============================================================================
 * Test Function Types
 * ============================================================================ */

typedef test_result_t (*test_func_t)(void);
typedef void (*setup_func_t)(void);
typedef void (*teardown_func_t)(void);

typedef struct {
    const char *name;
    test_func_t func;
} test_case_t;

typedef struct {
    const char *name;
    setup_func_t setup;
    teardown_func_t teardown;
    test_case_t *tests;
    uint32_t test_count;
} test_suite_t;

/* ============================================================================
 * Assertion Macros
 * ============================================================================ */

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            test_fail_at(__FILE__, __LINE__, #cond); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            test_fail_eq(__FILE__, __LINE__, #a, #b, (uint64_t)(a), (uint64_t)(b)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            test_fail_ne(__FILE__, __LINE__, #a, #b, (uint64_t)(a)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_GT(a, b) \
    do { \
        if ((a) <= (b)) { \
            test_fail_cmp(__FILE__, __LINE__, #a, ">", #b, (uint64_t)(a), (uint64_t)(b)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_GE(a, b) \
    do { \
        if ((a) < (b)) { \
            test_fail_cmp(__FILE__, __LINE__, #a, ">=", #b, (uint64_t)(a), (uint64_t)(b)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_LT(a, b) \
    do { \
        if ((a) >= (b)) { \
            test_fail_cmp(__FILE__, __LINE__, #a, "<", #b, (uint64_t)(a), (uint64_t)(b)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_LE(a, b) \
    do { \
        if ((a) > (b)) { \
            test_fail_cmp(__FILE__, __LINE__, #a, "<=", #b, (uint64_t)(a), (uint64_t)(b)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            test_fail_null(__FILE__, __LINE__, #ptr, false); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            test_fail_null(__FILE__, __LINE__, #ptr, true); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            test_fail_str(__FILE__, __LINE__, #a, #b, (a), (b)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_MEM_EQ(a, b, len) \
    do { \
        if (memcmp((a), (b), (len)) != 0) { \
            test_fail_mem(__FILE__, __LINE__, #a, #b, (len)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_SKIP_IF(cond, msg) \
    do { \
        if (cond) { \
            test_skip_reason(msg); \
            return TEST_SKIP; \
        } \
    } while (0)

/* ============================================================================
 * Test Framework API
 * ============================================================================ */

/**
 * test_init - Initialize test framework
 */
void test_init(void);

/**
 * test_run_suite - Run a test suite
 * @suite: Test suite to run
 * 
 * Returns suite statistics
 */
test_suite_stats_t test_run_suite(test_suite_t *suite);

/**
 * test_run_all - Run all registered test suites
 * 
 * Returns summary of all tests
 */
test_summary_t test_run_all(void);

/**
 * test_register_suite - Register a test suite
 * @suite: Test suite to register
 */
void test_register_suite(test_suite_t *suite);

/**
 * test_print_summary - Print test summary
 * @summary: Test summary to print
 */
void test_print_summary(test_summary_t *summary);

/* ============================================================================
 * Internal Assertion Helpers (used by macros)
 * ============================================================================ */

void test_fail_at(const char *file, int line, const char *cond);
void test_fail_eq(const char *file, int line, const char *a_str, const char *b_str,
                  uint64_t a, uint64_t b);
void test_fail_ne(const char *file, int line, const char *a_str, const char *b_str,
                  uint64_t val);
void test_fail_cmp(const char *file, int line, const char *a_str, const char *op,
                   const char *b_str, uint64_t a, uint64_t b);
void test_fail_null(const char *file, int line, const char *ptr_str, bool expected_not_null);
void test_fail_str(const char *file, int line, const char *a_str, const char *b_str,
                   const char *a, const char *b);
void test_fail_mem(const char *file, int line, const char *a_str, const char *b_str,
                   size_t len);
void test_skip_reason(const char *msg);

/* ============================================================================
 * Timing Utilities
 * ============================================================================ */

/**
 * test_get_time_us - Get current time in microseconds
 */
uint64_t test_get_time_us(void);

/**
 * test_measure_start - Start timing measurement
 */
void test_measure_start(void);

/**
 * test_measure_end - End timing measurement
 * 
 * Returns elapsed time in microseconds
 */
uint64_t test_measure_end(void);

#endif /* _PUREVISOR_TEST_FRAMEWORK_H */
