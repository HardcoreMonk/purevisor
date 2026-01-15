/*
 * PureVisor - Benchmark Suite Implementation
 * 
 * Performance benchmarking utilities
 */

#include <lib/types.h>
#include <lib/string.h>
#include <test/benchmark.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>
#include <mm/pmm.h>
#include <mm/heap.h>

/* TSC to microseconds (assume ~2GHz) */
#define TSC_TO_US   2000

/* ============================================================================
 * Benchmark Helpers
 * ============================================================================ */

static inline uint64_t get_time_us(void)
{
    return rdtsc() / TSC_TO_US;
}

bench_result_t bench_run(benchmark_t *bench)
{
    bench_result_t result = {0};
    result.name = bench->name;
    result.iterations = bench->iterations;
    result.min_us = UINT64_MAX;
    
    uint64_t total_start = get_time_us();
    
    for (uint64_t i = 0; i < bench->iterations; i++) {
        uint64_t start = rdtsc();
        bench->func();
        uint64_t end = rdtsc();
        
        uint64_t elapsed = (end - start) / TSC_TO_US;
        
        if (elapsed < result.min_us) result.min_us = elapsed;
        if (elapsed > result.max_us) result.max_us = elapsed;
    }
    
    result.total_us = get_time_us() - total_start;
    result.avg_us = result.total_us / bench->iterations;
    
    if (result.avg_us > 0) {
        result.ops_per_sec = 1000000ULL / result.avg_us;
    } else {
        result.ops_per_sec = 1000000ULL;  /* >1M ops/sec */
    }
    
    return result;
}

void bench_print_result(bench_result_t *result)
{
    kprintf("  %-30s %8llu its, avg %6llu us, min %6llu, max %6llu, %llu ops/s\n",
            result->name, result->iterations,
            result->avg_us, result->min_us, result->max_us,
            result->ops_per_sec);
}

/* ============================================================================
 * Memory Benchmarks
 * ============================================================================ */

static void bench_pmm_alloc_free(void)
{
    phys_addr_t addr = pmm_alloc_pages(1);
    if (addr) {
        pmm_free_pages(addr, 1);
    }
}

static void bench_kmalloc_small(void)
{
    void *p = kmalloc(64, GFP_KERNEL);
    if (p) kfree(p);
}

static void bench_kmalloc_medium(void)
{
    void *p = kmalloc(512, GFP_KERNEL);
    if (p) kfree(p);
}

static void bench_kmalloc_large(void)
{
    void *p = kmalloc(4096, GFP_KERNEL);
    if (p) kfree(p);
}

static void bench_memcpy_small(void)
{
    static char src[64] __attribute__((aligned(64)));
    static char dst[64] __attribute__((aligned(64)));
    memcpy(dst, src, 64);
}

static void bench_memcpy_medium(void)
{
    static char src[1024] __attribute__((aligned(64)));
    static char dst[1024] __attribute__((aligned(64)));
    memcpy(dst, src, 1024);
}

static void bench_memcpy_large(void)
{
    static char src[4096] __attribute__((aligned(64)));
    static char dst[4096] __attribute__((aligned(64)));
    memcpy(dst, src, 4096);
}

static void bench_memset_page(void)
{
    static char buf[4096] __attribute__((aligned(64)));
    memset(buf, 0, 4096);
}

void bench_memory(void)
{
    kprintf("\n[Memory Benchmarks]\n");
    kprintf("========================================\n");
    
    benchmark_t benchmarks[] = {
        {"pmm_alloc_free(1 page)", bench_pmm_alloc_free, 10000},
        {"kmalloc/kfree(64B)", bench_kmalloc_small, 100000},
        {"kmalloc/kfree(512B)", bench_kmalloc_medium, 100000},
        {"kmalloc/kfree(4KB)", bench_kmalloc_large, 10000},
        {"memcpy(64B)", bench_memcpy_small, 1000000},
        {"memcpy(1KB)", bench_memcpy_medium, 100000},
        {"memcpy(4KB)", bench_memcpy_large, 100000},
        {"memset(4KB)", bench_memset_page, 100000},
    };
    
    for (size_t i = 0; i < sizeof(benchmarks)/sizeof(benchmarks[0]); i++) {
        bench_result_t result = bench_run(&benchmarks[i]);
        bench_print_result(&result);
    }
    
    kprintf("========================================\n");
}

/* ============================================================================
 * VMX Benchmarks
 * ============================================================================ */

/* CPUID benchmark - common VM exit cause */
static void bench_cpuid(void)
{
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
}

/* RDTSC benchmark */
static void bench_rdtsc_call(void)
{
    (void)rdtsc();
}

/* RDMSR benchmark (TSC) */
static void bench_rdmsr(void)
{
    rdmsr(0x10);  /* TSC */
}

void bench_vmx(void)
{
    kprintf("\n[CPU/VMX Benchmarks]\n");
    kprintf("========================================\n");
    
    benchmark_t benchmarks[] = {
        {"CPUID(0)", bench_cpuid, 1000000},
        {"RDTSC", bench_rdtsc_call, 10000000},
        {"RDMSR(TSC)", bench_rdmsr, 1000000},
    };
    
    for (size_t i = 0; i < sizeof(benchmarks)/sizeof(benchmarks[0]); i++) {
        bench_result_t result = bench_run(&benchmarks[i]);
        bench_print_result(&result);
    }
    
    kprintf("========================================\n");
}

/* ============================================================================
 * Storage Benchmarks (using memory operations as proxy)
 * ============================================================================ */

static char storage_buf[4096] __attribute__((aligned(4096)));

static void bench_storage_read_4k(void)
{
    volatile char sum = 0;
    for (int i = 0; i < 4096; i += 64) {
        sum += storage_buf[i];
    }
    (void)sum;
}

static void bench_storage_write_4k(void)
{
    memset(storage_buf, 0xAA, 4096);
}

void bench_storage(void)
{
    kprintf("\n[Storage Benchmarks (simulated)]\n");
    kprintf("========================================\n");
    
    benchmark_t benchmarks[] = {
        {"read 4KB (sequential)", bench_storage_read_4k, 100000},
        {"write 4KB (sequential)", bench_storage_write_4k, 100000},
    };
    
    for (size_t i = 0; i < sizeof(benchmarks)/sizeof(benchmarks[0]); i++) {
        bench_result_t result = bench_run(&benchmarks[i]);
        bench_print_result(&result);
    }
    
    kprintf("========================================\n");
}

/* ============================================================================
 * Run All Benchmarks
 * ============================================================================ */

void bench_all(void)
{
    kprintf("\n");
    kprintf("########################################\n");
    kprintf("#    PureVisor Benchmark Suite        #\n");
    kprintf("########################################\n");
    
    bench_memory();
    bench_vmx();
    bench_storage();
    
    kprintf("\n");
    kprintf("########################################\n");
    kprintf("#    Benchmarks Complete              #\n");
    kprintf("########################################\n\n");
}
