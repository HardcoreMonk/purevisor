/*
 * PureVisor - SMP Implementation
 * 
 * Symmetric Multi-Processing support
 */

#include <lib/types.h>
#include <lib/string.h>
#include <kernel/smp.h>
#include <kernel/apic.h>
#include <kernel/console.h>
#include <mm/pmm.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * AP Trampoline Code
 * ============================================================================ */

/* This code will be copied to low memory for AP startup */
extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];

/* ============================================================================
 * Global State
 * ============================================================================ */

static percpu_t percpu_data[MAX_CPUS];
static uint32_t cpu_count = 1;          /* BSP is always present */
static uint32_t online_cpus = 1;
static volatile uint32_t ap_started = 0;
static volatile uint32_t smp_lock = 0;

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static void delay_us(uint32_t us)
{
    /* Simple delay using busy loop - not accurate but sufficient */
    volatile uint64_t count = us * 1000;
    while (count--) {
        __asm__ __volatile__("pause");
    }
}

/* Initialize per-CPU data for a CPU */
static void init_percpu(uint32_t cpu_id, uint32_t apic_id, bool is_bsp)
{
    percpu_t *cpu = &percpu_data[cpu_id];
    
    memset(cpu, 0, sizeof(percpu_t));
    cpu->cpu_id = cpu_id;
    cpu->apic_id = apic_id;
    cpu->state = is_bsp ? CPU_STATE_ONLINE : CPU_STATE_OFFLINE;
    cpu->is_bsp = is_bsp;
    
    /* Allocate kernel stack for this CPU */
    if (!is_bsp) {
        phys_addr_t stack_phys = pmm_alloc_pages(2);  /* 16KB stack */
        if (stack_phys) {
            cpu->kernel_stack = phys_to_virt(stack_phys) + (4 * PAGE_SIZE);
            cpu->kernel_stack_size = 4 * PAGE_SIZE;
        }
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void smp_init(void)
{
    pr_info("SMP: Initializing multi-processor support...");
    
    /* Initialize BSP (CPU 0) */
    uint32_t bsp_apic_id = lapic_get_id();
    init_percpu(0, bsp_apic_id, true);
    
    pr_info("SMP: BSP APIC ID: %u", bsp_apic_id);
    
    /* 
     * In a full implementation, we would:
     * 1. Parse ACPI MADT table to find all CPUs
     * 2. Or parse MP Floating Pointer Structure
     * 
     * For now, we'll detect CPUs by trying to start them
     * and seeing which ones respond.
     */
    
    /* Try to detect additional CPUs (simplified) */
    /* In QEMU with -smp N, CPUs have APIC IDs 0, 1, 2, ... N-1 */
    
    for (uint32_t apic_id = 0; apic_id < 8; apic_id++) {
        if (apic_id == bsp_apic_id) continue;
        
        /* Assume CPU exists if APIC ID is sequential */
        if (cpu_count < MAX_CPUS) {
            init_percpu(cpu_count, apic_id, false);
            cpu_count++;
        }
    }
    
    pr_info("SMP: Detected %u CPU(s)", cpu_count);
}

void smp_start_aps(void)
{
    if (cpu_count <= 1) {
        pr_info("SMP: No APs to start (single CPU system)");
        return;
    }
    
    pr_info("SMP: Starting Application Processors...");
    
    /*
     * AP Startup Sequence (INIT-SIPI-SIPI):
     * 1. Copy trampoline code to low memory (below 1MB)
     * 2. Send INIT IPI to target CPU
     * 3. Wait 10ms
     * 4. Send STARTUP IPI with vector pointing to trampoline
     * 5. Wait 200us
     * 6. Send second STARTUP IPI
     * 7. Wait for AP to set started flag
     */
    
    /* For now, just simulate starting APs since we don't have
     * the trampoline code set up yet */
    
    for (uint32_t i = 1; i < cpu_count; i++) {
        percpu_t *cpu = &percpu_data[i];
        
        pr_info("SMP: Starting CPU %u (APIC ID %u)...", i, cpu->apic_id);
        
        /* Mark as starting */
        cpu->state = CPU_STATE_STARTING;
        
        /* Send INIT IPI */
        lapic_send_init(cpu->apic_id);
        delay_us(10000);  /* 10ms delay */
        
        /* Send first STARTUP IPI */
        /* Vector is page number of trampoline code (e.g., 0x08 for 0x8000) */
        lapic_send_startup(cpu->apic_id, AP_BOOT_ADDR >> 12);
        delay_us(200);
        
        /* Send second STARTUP IPI */
        lapic_send_startup(cpu->apic_id, AP_BOOT_ADDR >> 12);
        delay_us(200);
        
        /* Wait for AP to come online (with timeout) */
        uint32_t timeout = 1000;
        while (cpu->state != CPU_STATE_ONLINE && timeout > 0) {
            delay_us(1000);
            timeout--;
        }
        
        if (cpu->state == CPU_STATE_ONLINE) {
            online_cpus++;
            pr_info("SMP: CPU %u online", i);
        } else {
            cpu->state = CPU_STATE_OFFLINE;
            pr_warn("SMP: CPU %u failed to start", i);
        }
    }
    
    pr_info("SMP: %u of %u CPUs online", online_cpus, cpu_count);
}

uint32_t smp_get_cpu_count(void)
{
    return cpu_count;
}

uint32_t smp_get_online_count(void)
{
    return online_cpus;
}

uint32_t smp_get_current_cpu(void)
{
    /* Get current APIC ID and find matching CPU */
    uint32_t apic_id = lapic_get_id();
    
    for (uint32_t i = 0; i < cpu_count; i++) {
        if (percpu_data[i].apic_id == apic_id) {
            return i;
        }
    }
    
    return 0;  /* Default to BSP */
}

percpu_t *smp_get_percpu(int cpu_id)
{
    if (cpu_id < 0) {
        cpu_id = smp_get_current_cpu();
    }
    
    if ((uint32_t)cpu_id >= cpu_count) {
        return NULL;
    }
    
    return &percpu_data[cpu_id];
}

void smp_broadcast_ipi(uint32_t vector)
{
    lapic_send_ipi_all(vector, false);
}

void smp_send_ipi(uint32_t cpu_id, uint32_t vector)
{
    if (cpu_id >= cpu_count) return;
    
    lapic_send_ipi(percpu_data[cpu_id].apic_id, vector);
}

/* ============================================================================
 * IRQ Lock Implementation
 * ============================================================================ */

void irqlock_acquire(irqlock_t *lock)
{
    /* Save flags and disable interrupts */
    uint64_t flags;
    __asm__ __volatile__(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
    );
    
    /* Acquire spinlock */
    spinlock_acquire(&lock->lock);
    
    lock->flags = flags;
}

void irqlock_release(irqlock_t *lock)
{
    uint64_t flags = lock->flags;
    
    /* Release spinlock */
    spinlock_release(&lock->lock);
    
    /* Restore flags (including interrupt state) */
    __asm__ __volatile__(
        "pushq %0\n\t"
        "popfq"
        :: "r"(flags)
        : "cc"
    );
}

/* ============================================================================
 * AP Entry Point (called from trampoline)
 * ============================================================================ */

void ap_entry(void)
{
    /* Get our CPU ID */
    uint32_t cpu_id = smp_get_current_cpu();
    percpu_t *cpu = &percpu_data[cpu_id];
    
    /* Initialize local APIC for this CPU */
    lapic_enable();
    
    /* Mark as online */
    cpu->state = CPU_STATE_ONLINE;
    __sync_fetch_and_add(&ap_started, 1);
    
    pr_info("SMP: AP %u (APIC %u) started", cpu_id, cpu->apic_id);
    
    /* Enable interrupts and enter idle loop */
    __asm__ __volatile__("sti");
    
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
