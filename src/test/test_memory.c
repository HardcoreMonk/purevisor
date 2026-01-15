/*
 * PureVisor - Memory Test Suite
 * 
 * Unit tests for PMM, Heap, and Paging subsystems
 */

#include <lib/types.h>
#include <lib/string.h>
#include <test/framework.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <mm/paging.h>

/* ============================================================================
 * PMM Tests
 * ============================================================================ */

static test_result_t test_pmm_single_page_alloc(void)
{
    phys_addr_t addr = pmm_alloc_pages(1);
    TEST_ASSERT_NE(addr, 0);
    TEST_ASSERT((addr & 0xFFF) == 0);  /* Page aligned */
    
    pmm_free_pages(addr, 1);
    return TEST_PASS;
}

static test_result_t test_pmm_multi_page_alloc(void)
{
    phys_addr_t addr = pmm_alloc_pages(4);
    TEST_ASSERT_NE(addr, 0);
    TEST_ASSERT((addr & 0xFFF) == 0);  /* Page aligned */
    
    pmm_free_pages(addr, 4);
    return TEST_PASS;
}

static test_result_t test_pmm_large_alloc(void)
{
    /* Try to allocate 1MB (256 pages) */
    phys_addr_t addr = pmm_alloc_pages(256);
    if (addr == 0) {
        /* Not enough memory, skip */
        test_skip_reason("Insufficient memory for 1MB allocation");
        return TEST_SKIP;
    }
    
    TEST_ASSERT((addr & 0xFFF) == 0);
    pmm_free_pages(addr, 256);
    return TEST_PASS;
}

static test_result_t test_pmm_alloc_free_cycle(void)
{
    phys_addr_t addrs[10];
    
    /* Allocate 10 pages */
    for (int i = 0; i < 10; i++) {
        addrs[i] = pmm_alloc_pages(1);
        TEST_ASSERT_NE(addrs[i], 0);
    }
    
    /* Free them */
    for (int i = 0; i < 10; i++) {
        pmm_free_pages(addrs[i], 1);
    }
    
    return TEST_PASS;
}

static test_result_t test_pmm_zero_alloc(void)
{
    /* Allocating 0 pages should return NULL or handle gracefully */
    phys_addr_t addr = pmm_alloc_pages(0);
    /* Either NULL or valid is acceptable, just shouldn't crash */
    (void)addr;
    return TEST_PASS;
}

static test_case_t pmm_tests[] = {
    {"single_page_alloc", test_pmm_single_page_alloc},
    {"multi_page_alloc", test_pmm_multi_page_alloc},
    {"large_alloc", test_pmm_large_alloc},
    {"alloc_free_cycle", test_pmm_alloc_free_cycle},
    {"zero_alloc", test_pmm_zero_alloc},
};

static test_suite_t pmm_suite = {
    .name = "PMM (Physical Memory Manager)",
    .setup = NULL,
    .teardown = NULL,
    .tests = pmm_tests,
    .test_count = sizeof(pmm_tests) / sizeof(pmm_tests[0]),
};

/* ============================================================================
 * Heap Tests
 * ============================================================================ */

static test_result_t test_heap_small_alloc(void)
{
    void *p = kmalloc(32, GFP_KERNEL);
    TEST_ASSERT_NOT_NULL(p);
    
    /* Write to verify it's usable */
    memset(p, 0xAA, 32);
    
    kfree(p);
    return TEST_PASS;
}

static test_result_t test_heap_medium_alloc(void)
{
    void *p = kmalloc(512, GFP_KERNEL);
    TEST_ASSERT_NOT_NULL(p);
    
    memset(p, 0xBB, 512);
    
    kfree(p);
    return TEST_PASS;
}

static test_result_t test_heap_large_alloc(void)
{
    void *p = kmalloc(4096, GFP_KERNEL);
    TEST_ASSERT_NOT_NULL(p);
    
    memset(p, 0xCC, 4096);
    
    kfree(p);
    return TEST_PASS;
}

static test_result_t test_heap_zero_alloc(void)
{
    void *p = kmalloc(64, GFP_KERNEL | GFP_ZERO);
    TEST_ASSERT_NOT_NULL(p);
    
    /* Verify it's zeroed */
    uint8_t *bytes = (uint8_t *)p;
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQ(bytes[i], 0);
    }
    
    kfree(p);
    return TEST_PASS;
}

static test_result_t test_heap_realloc_pattern(void)
{
    /* Allocate, free, allocate again - should work */
    void *p1 = kmalloc(128, GFP_KERNEL);
    TEST_ASSERT_NOT_NULL(p1);
    kfree(p1);
    
    void *p2 = kmalloc(128, GFP_KERNEL);
    TEST_ASSERT_NOT_NULL(p2);
    kfree(p2);
    
    return TEST_PASS;
}

static test_result_t test_heap_multiple_sizes(void)
{
    void *ptrs[8];
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    
    /* Allocate different sizes */
    for (int i = 0; i < 8; i++) {
        ptrs[i] = kmalloc(sizes[i], GFP_KERNEL);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }
    
    /* Free in reverse order */
    for (int i = 7; i >= 0; i--) {
        kfree(ptrs[i]);
    }
    
    return TEST_PASS;
}

static test_result_t test_heap_alignment(void)
{
    void *p = kmalloc(64, GFP_KERNEL);
    TEST_ASSERT_NOT_NULL(p);
    
    /* Should be at least 8-byte aligned */
    TEST_ASSERT(((uintptr_t)p & 0x7) == 0);
    
    kfree(p);
    return TEST_PASS;
}

static test_case_t heap_tests[] = {
    {"small_alloc (32B)", test_heap_small_alloc},
    {"medium_alloc (512B)", test_heap_medium_alloc},
    {"large_alloc (4KB)", test_heap_large_alloc},
    {"zero_alloc", test_heap_zero_alloc},
    {"realloc_pattern", test_heap_realloc_pattern},
    {"multiple_sizes", test_heap_multiple_sizes},
    {"alignment", test_heap_alignment},
};

static test_suite_t heap_suite = {
    .name = "Heap (kmalloc/kfree)",
    .setup = NULL,
    .teardown = NULL,
    .tests = heap_tests,
    .test_count = sizeof(heap_tests) / sizeof(heap_tests[0]),
};

/* ============================================================================
 * Paging Tests
 * ============================================================================ */

static test_result_t test_paging_phys_to_virt(void)
{
    /* Test identity mapping region */
    void *virt = phys_to_virt(0x1000);
    TEST_ASSERT_NOT_NULL(virt);
    
    phys_addr_t phys = virt_to_phys(virt);
    TEST_ASSERT_EQ(phys, 0x1000);
    
    return TEST_PASS;
}

static test_result_t test_paging_kernel_mapping(void)
{
    /* Kernel should be mapped in higher half */
    extern void kernel_main(uint32_t, void*);
    phys_addr_t kernel_phys = virt_to_phys((void*)kernel_main);
    
    /* Physical address should be in low memory */
    TEST_ASSERT_LT(kernel_phys, 0x100000000ULL);  /* < 4GB */
    
    return TEST_PASS;
}

static test_case_t paging_tests[] = {
    {"phys_to_virt", test_paging_phys_to_virt},
    {"kernel_mapping", test_paging_kernel_mapping},
};

static test_suite_t paging_suite = {
    .name = "Paging (Virtual Memory)",
    .setup = NULL,
    .teardown = NULL,
    .tests = paging_tests,
    .test_count = sizeof(paging_tests) / sizeof(paging_tests[0]),
};

/* ============================================================================
 * Suite Registration
 * ============================================================================ */

void test_pmm_suite(void)
{
    test_register_suite(&pmm_suite);
}

void test_heap_suite(void)
{
    test_register_suite(&heap_suite);
}

void test_paging_suite(void)
{
    test_register_suite(&paging_suite);
}
