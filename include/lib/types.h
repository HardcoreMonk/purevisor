/*
 * PureVisor - Types Header
 * 
 * Basic type definitions for freestanding environment
 * No libc dependency
 */

#ifndef _PUREVISOR_TYPES_H
#define _PUREVISOR_TYPES_H

/* ============================================================================
 * Fixed-width integer types
 * ============================================================================ */

typedef signed char         int8_t;
typedef unsigned char       uint8_t;
typedef signed short        int16_t;
typedef unsigned short      uint16_t;
typedef signed int          int32_t;
typedef unsigned int        uint32_t;
typedef signed long long    int64_t;
typedef unsigned long long  uint64_t;

/* Size types */
typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef int64_t             ptrdiff_t;

/* Pointer-sized integers */
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;

/* ============================================================================
 * Boolean type
 * ============================================================================ */

typedef _Bool               bool;
#define true                1
#define false               0

/* ============================================================================
 * NULL pointer
 * ============================================================================ */

#define NULL                ((void *)0)

/* ============================================================================
 * Limits
 * ============================================================================ */

#define INT8_MIN            (-128)
#define INT8_MAX            (127)
#define UINT8_MAX           (255)

#define INT16_MIN           (-32768)
#define INT16_MAX           (32767)
#define UINT16_MAX          (65535)

#define INT32_MIN           (-2147483647 - 1)
#define INT32_MAX           (2147483647)
#define UINT32_MAX          (4294967295U)

#define INT64_MIN           (-9223372036854775807LL - 1)
#define INT64_MAX           (9223372036854775807LL)
#define UINT64_MAX          (18446744073709551615ULL)

#define SIZE_MAX            UINT64_MAX
#define PTRDIFF_MIN         INT64_MIN
#define PTRDIFF_MAX         INT64_MAX

/* ============================================================================
 * Physical and Virtual Address Types
 * ============================================================================ */

typedef uint64_t            phys_addr_t;    /* Physical address */
typedef uint64_t            virt_addr_t;    /* Virtual address */

/* ============================================================================
 * Alignment and Attribute Macros
 * ============================================================================ */

#define ALIGNED(x)          __attribute__((aligned(x)))
#define PACKED              __attribute__((packed))
#define UNUSED              __attribute__((unused))
#define NORETURN            __attribute__((noreturn))
#define ALWAYS_INLINE       __attribute__((always_inline)) inline
#define NOINLINE            __attribute__((noinline))
#define SECTION(x)          __attribute__((section(x)))
#define WEAK                __attribute__((weak))

/* ============================================================================
 * Memory Size Constants
 * ============================================================================ */

#define KB                  (1024ULL)
#define MB                  (1024ULL * KB)
#define GB                  (1024ULL * MB)
#define TB                  (1024ULL * GB)

/* Page sizes */
#define PAGE_SIZE           (4 * KB)
#define PAGE_SIZE_2M        (2 * MB)
#define PAGE_SIZE_1G        (1 * GB)

#define PAGE_SHIFT          12
#define PAGE_MASK           (~(PAGE_SIZE - 1))

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#define ALIGN_UP(x, a)      (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)    ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)    (((x) & ((a) - 1)) == 0)

#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)    MIN(MAX(x, lo), hi)

#define BIT(n)              (1ULL << (n))
#define BITS(hi, lo)        ((BIT((hi) - (lo) + 1) - 1) << (lo))

/* ============================================================================
 * Register Access Macros
 * ============================================================================ */

#define READ_ONCE(x)        (*(volatile typeof(x) *)&(x))
#define WRITE_ONCE(x, val)  (*(volatile typeof(x) *)&(x) = (val))

/* Memory-mapped I/O */
#define MMIO_READ8(addr)    (*(volatile uint8_t *)(addr))
#define MMIO_READ16(addr)   (*(volatile uint16_t *)(addr))
#define MMIO_READ32(addr)   (*(volatile uint32_t *)(addr))
#define MMIO_READ64(addr)   (*(volatile uint64_t *)(addr))

#define MMIO_WRITE8(addr, val)  (*(volatile uint8_t *)(addr) = (val))
#define MMIO_WRITE16(addr, val) (*(volatile uint16_t *)(addr) = (val))
#define MMIO_WRITE32(addr, val) (*(volatile uint32_t *)(addr) = (val))
#define MMIO_WRITE64(addr, val) (*(volatile uint64_t *)(addr) = (val))

/* ============================================================================
 * Compiler Barriers
 * ============================================================================ */

#define barrier()           __asm__ __volatile__("" ::: "memory")
#define mb()                __asm__ __volatile__("mfence" ::: "memory")
#define rmb()               __asm__ __volatile__("lfence" ::: "memory")
#define wmb()               __asm__ __volatile__("sfence" ::: "memory")

/* ============================================================================
 * Static Assertions
 * ============================================================================ */

#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/* Verify type sizes */
STATIC_ASSERT(sizeof(int8_t)  == 1, "int8_t must be 1 byte");
STATIC_ASSERT(sizeof(int16_t) == 2, "int16_t must be 2 bytes");
STATIC_ASSERT(sizeof(int32_t) == 4, "int32_t must be 4 bytes");
STATIC_ASSERT(sizeof(int64_t) == 8, "int64_t must be 8 bytes");
STATIC_ASSERT(sizeof(void *)  == 8, "pointer must be 8 bytes on x86_64");

#endif /* _PUREVISOR_TYPES_H */
