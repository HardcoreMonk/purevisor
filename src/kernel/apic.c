/*
 * PureVisor - Local APIC Implementation
 * 
 * Advanced Programmable Interrupt Controller
 */

#include <lib/types.h>
#include <lib/string.h>
#include <kernel/apic.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>
#include <mm/pmm.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static volatile uint32_t *lapic_base = NULL;
static volatile uint32_t *ioapic_base = NULL;
static bool apic_initialized = false;

/* ============================================================================
 * Local APIC Functions
 * ============================================================================ */

uint32_t lapic_read(uint32_t reg)
{
    return lapic_base[reg / 4];
}

void lapic_write(uint32_t reg, uint32_t value)
{
    lapic_base[reg / 4] = value;
    /* Read back to ensure write is complete */
    (void)lapic_base[LAPIC_ID / 4];
}

void lapic_init(void)
{
    pr_info("LAPIC: Initializing Local APIC...");
    
    /* Get APIC base from MSR */
    uint64_t apic_msr = rdmsr(MSR_IA32_APIC_BASE);
    phys_addr_t apic_phys = apic_msr & 0xFFFFF000;
    
    /* Check if APIC is enabled */
    if (!(apic_msr & (1 << 11))) {
        pr_warn("LAPIC: APIC disabled in MSR, enabling...");
        apic_msr |= (1 << 11);
        wrmsr(MSR_IA32_APIC_BASE, apic_msr);
    }
    
    /* Map APIC registers */
    lapic_base = (volatile uint32_t *)phys_to_virt(apic_phys);
    
    pr_info("LAPIC: Base address: 0x%llx", apic_phys);
    pr_info("LAPIC: ID: %u, Version: 0x%x", 
            lapic_read(LAPIC_ID) >> 24,
            lapic_read(LAPIC_VERSION) & 0xFF);
    
    lapic_enable();
    apic_initialized = true;
}

void lapic_enable(void)
{
    /* Set spurious interrupt vector and enable APIC */
    uint32_t svr = lapic_read(LAPIC_SVR);
    svr |= LAPIC_SVR_ENABLE;    /* Enable APIC */
    svr |= 0xFF;                 /* Spurious vector = 0xFF */
    lapic_write(LAPIC_SVR, svr);
    
    /* Clear error status */
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);
    
    /* Mask all LVT entries initially */
    lapic_write(LAPIC_TIMER_LVT, LVT_MASKED);
    lapic_write(LAPIC_LINT0_LVT, LVT_MASKED);
    lapic_write(LAPIC_LINT1_LVT, LVT_MASKED);
    lapic_write(LAPIC_ERROR_LVT, LVT_MASKED);
    
    /* Set task priority to 0 (accept all interrupts) */
    lapic_write(LAPIC_TPR, 0);
    
    pr_info("LAPIC: Enabled");
}

uint32_t lapic_get_id(void)
{
    return lapic_read(LAPIC_ID) >> 24;
}

void lapic_eoi(void)
{
    lapic_write(LAPIC_EOI, 0);
}

void lapic_send_ipi(uint32_t dest, uint32_t vector)
{
    /* Wait for previous IPI to complete */
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        __asm__ __volatile__("pause");
    }
    
    /* Set destination */
    lapic_write(LAPIC_ICR_HIGH, dest << 24);
    
    /* Send IPI: fixed delivery, edge triggered, assert */
    lapic_write(LAPIC_ICR_LOW, vector | ICR_FIXED | ICR_PHYSICAL | 
                ICR_ASSERT | ICR_EDGE | ICR_NO_SHORTHAND);
}

void lapic_send_ipi_all(uint32_t vector, bool include_self)
{
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        __asm__ __volatile__("pause");
    }
    
    uint32_t shorthand = include_self ? (2 << 18) : ICR_ALL_EXCL;
    lapic_write(LAPIC_ICR_LOW, vector | ICR_FIXED | ICR_ASSERT | 
                ICR_EDGE | shorthand);
}

void lapic_send_init(uint32_t dest)
{
    /* Wait for previous IPI */
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        __asm__ __volatile__("pause");
    }
    
    /* Set destination */
    lapic_write(LAPIC_ICR_HIGH, dest << 24);
    
    /* Send INIT IPI: assert */
    lapic_write(LAPIC_ICR_LOW, ICR_INIT | ICR_PHYSICAL | ICR_ASSERT | 
                ICR_LEVEL | ICR_NO_SHORTHAND);
    
    /* Wait for delivery */
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        __asm__ __volatile__("pause");
    }
    
    /* Deassert */
    lapic_write(LAPIC_ICR_LOW, ICR_INIT | ICR_PHYSICAL | ICR_LEVEL | 
                ICR_NO_SHORTHAND);
}

void lapic_send_startup(uint32_t dest, uint8_t vector)
{
    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12)) {
        __asm__ __volatile__("pause");
    }
    
    lapic_write(LAPIC_ICR_HIGH, dest << 24);
    lapic_write(LAPIC_ICR_LOW, vector | ICR_STARTUP | ICR_PHYSICAL | 
                ICR_ASSERT | ICR_EDGE | ICR_NO_SHORTHAND);
}

void lapic_timer_init(uint8_t vector, bool periodic)
{
    /* Set divider to 16 */
    lapic_write(LAPIC_TIMER_DCR, TIMER_DIV_16);
    
    /* Configure timer LVT */
    uint32_t lvt = vector;
    if (periodic) lvt |= TIMER_PERIODIC;
    lapic_write(LAPIC_TIMER_LVT, lvt);
    
    pr_info("LAPIC: Timer initialized (vector %u, %s)", 
            vector, periodic ? "periodic" : "one-shot");
}

void lapic_timer_set(uint32_t count)
{
    lapic_write(LAPIC_TIMER_ICR, count);
}

/* ============================================================================
 * I/O APIC Functions
 * ============================================================================ */

uint32_t ioapic_read(uint32_t reg)
{
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    return ioapic_base[IOAPIC_WINDOW / 4];
}

void ioapic_write(uint32_t reg, uint32_t value)
{
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    ioapic_base[IOAPIC_WINDOW / 4] = value;
}

void ioapic_init(void)
{
    pr_info("IOAPIC: Initializing I/O APIC...");
    
    /* Map I/O APIC registers */
    ioapic_base = (volatile uint32_t *)phys_to_virt(IOAPIC_DEFAULT_BASE);
    
    uint32_t id = (ioapic_read(IOAPIC_ID) >> 24) & 0x0F;
    uint32_t ver = ioapic_read(IOAPIC_VER);
    uint32_t max_redir = ((ver >> 16) & 0xFF) + 1;
    
    pr_info("IOAPIC: ID: %u, Version: 0x%x, Max IRQs: %u", 
            id, ver & 0xFF, max_redir);
    
    /* Mask all IRQs initially */
    for (uint32_t i = 0; i < max_redir; i++) {
        ioapic_mask_irq(i);
    }
    
    pr_info("IOAPIC: Initialized");
}

void ioapic_set_irq(uint8_t irq, uint8_t vector, uint8_t dest, uint32_t flags)
{
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    
    /* Low 32 bits: vector, delivery mode, etc. */
    uint32_t low = vector | flags;
    
    /* High 32 bits: destination */
    uint32_t high = (uint32_t)dest << 24;
    
    ioapic_write(reg, low);
    ioapic_write(reg + 1, high);
}

void ioapic_mask_irq(uint8_t irq)
{
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    uint32_t val = ioapic_read(reg);
    ioapic_write(reg, val | IOAPIC_MASKED);
}

void ioapic_unmask_irq(uint8_t irq)
{
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    uint32_t val = ioapic_read(reg);
    ioapic_write(reg, val & ~IOAPIC_MASKED);
}
