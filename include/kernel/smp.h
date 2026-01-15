/*
 * PureVisor - SMP (Symmetric Multi-Processing) Header
 * 
 * Multi-core CPU support
 */

#ifndef _PUREVISOR_SMP_H
#define _PUREVISOR_SMP_H

#include <lib/types.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CPUS            256         /* Maximum supported CPUs */
#define AP_BOOT_ADDR        0x8000      /* AP trampoline address */

/* CPU states */
typedef enum {
    CPU_STATE_OFFLINE,
    CPU_STATE_STARTING,
    CPU_STATE_ONLINE,
    CPU_STATE_HALTED
} cpu_state_t;

/* ============================================================================
 * Per-CPU Data Structure
 * ============================================================================ */

typedef struct {
    uint32_t cpu_id;                /* Logical CPU ID */
    uint32_t apic_id;               /* Local APIC ID */
    cpu_state_t state;              /* CPU state */
    bool is_bsp;                    /* Is Bootstrap Processor */
    
    /* Stack */
    void *kernel_stack;             /* Kernel stack top */
    uint64_t kernel_stack_size;
    
    /* Current task (for future scheduler) */
    void *current_task;
    
    /* Statistics */
    uint64_t ticks;                 /* Timer ticks */
    uint64_t idle_ticks;            /* Idle ticks */
    
    /* Per-CPU GDT and TSS */
    void *gdt;
    void *tss;
} percpu_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * smp_init - Initialize SMP support
 * 
 * Detects CPUs and prepares for AP startup
 */
void smp_init(void);

/**
 * smp_start_aps - Start Application Processors
 * 
 * Sends INIT-SIPI-SIPI sequence to all APs
 */
void smp_start_aps(void);

/**
 * smp_get_cpu_count - Get number of CPUs
 */
uint32_t smp_get_cpu_count(void);

/**
 * smp_get_online_count - Get number of online CPUs
 */
uint32_t smp_get_online_count(void);

/**
 * smp_get_current_cpu - Get current CPU ID
 */
uint32_t smp_get_current_cpu(void);

/**
 * smp_get_percpu - Get per-CPU data
 * @cpu_id: CPU ID (or -1 for current)
 */
percpu_t *smp_get_percpu(int cpu_id);

/**
 * smp_get_current_percpu - Get current CPU's per-CPU data
 */
static inline percpu_t *smp_get_current_percpu(void)
{
    return smp_get_percpu(-1);
}

/**
 * smp_broadcast_ipi - Send IPI to all other CPUs
 * @vector: Interrupt vector
 */
void smp_broadcast_ipi(uint32_t vector);

/**
 * smp_send_ipi - Send IPI to specific CPU
 * @cpu_id: Target CPU ID
 * @vector: Interrupt vector
 */
void smp_send_ipi(uint32_t cpu_id, uint32_t vector);

/* ============================================================================
 * Per-CPU Variable Macros
 * ============================================================================ */

#define DEFINE_PERCPU(type, name) \
    __attribute__((section(".percpu"))) type name

#define get_percpu(name) \
    (*({ percpu_t *__p = smp_get_current_percpu(); &__p->name; }))

/* ============================================================================
 * Spinlock (for SMP synchronization)
 * ============================================================================ */

typedef volatile uint32_t spinlock_t;

#define SPINLOCK_INIT       0
#define SPINLOCK_LOCKED     1

static inline void spinlock_init(spinlock_t *lock)
{
    *lock = SPINLOCK_INIT;
}

static inline void spinlock_acquire(spinlock_t *lock)
{
    while (__sync_lock_test_and_set(lock, SPINLOCK_LOCKED)) {
        while (*lock) {
            __asm__ __volatile__("pause" ::: "memory");
        }
    }
}

static inline void spinlock_release(spinlock_t *lock)
{
    __sync_lock_release(lock);
}

static inline bool spinlock_try_acquire(spinlock_t *lock)
{
    return __sync_lock_test_and_set(lock, SPINLOCK_LOCKED) == 0;
}

/* Interrupt-safe spinlock */
typedef struct {
    spinlock_t lock;
    uint64_t flags;
} irqlock_t;

void irqlock_acquire(irqlock_t *lock);
void irqlock_release(irqlock_t *lock);

#endif /* _PUREVISOR_SMP_H */
