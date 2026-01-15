/*
 * PureVisor - Benchmark Suite Header
 * 
 * Performance benchmarking utilities
 */

#ifndef _PUREVISOR_TEST_BENCHMARK_H
#define _PUREVISOR_TEST_BENCHMARK_H

#include <lib/types.h>

/* ============================================================================
 * Benchmark Types
 * ============================================================================ */

typedef struct {
    const char *name;
    uint64_t iterations;
    uint64_t total_us;
    uint64_t min_us;
    uint64_t max_us;
    uint64_t avg_us;
    uint64_t ops_per_sec;
} bench_result_t;

typedef void (*bench_func_t)(void);

typedef struct {
    const char *name;
    bench_func_t func;
    uint64_t iterations;
} benchmark_t;

/* ============================================================================
 * Benchmark API
 * ============================================================================ */

/**
 * bench_run - Run a single benchmark
 * @bench: Benchmark to run
 * 
 * Returns benchmark results
 */
bench_result_t bench_run(benchmark_t *bench);

/**
 * bench_print_result - Print benchmark result
 * @result: Result to print
 */
void bench_print_result(bench_result_t *result);

/* ============================================================================
 * Standard Benchmarks
 * ============================================================================ */

/**
 * bench_memory - Run memory benchmarks
 */
void bench_memory(void);

/**
 * bench_vmx - Run VMX benchmarks
 */
void bench_vmx(void);

/**
 * bench_storage - Run storage benchmarks
 */
void bench_storage(void);

/**
 * bench_all - Run all benchmarks
 */
void bench_all(void);

#endif /* _PUREVISOR_TEST_BENCHMARK_H */
