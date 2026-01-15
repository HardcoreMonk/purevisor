/*
 * PureVisor - Local APIC Header
 * 
 * Advanced Programmable Interrupt Controller
 */

#ifndef _PUREVISOR_APIC_H
#define _PUREVISOR_APIC_H

#include <lib/types.h>

/* ============================================================================
 * APIC Register Offsets
 * ============================================================================ */

#define LAPIC_ID            0x020
#define LAPIC_VERSION       0x030
#define LAPIC_TPR           0x080
#define LAPIC_EOI           0x0B0
#define LAPIC_LDR           0x0D0
#define LAPIC_DFR           0x0E0
#define LAPIC_SVR           0x0F0
#define LAPIC_ISR           0x100
#define LAPIC_ESR           0x280
#define LAPIC_ICR_LOW       0x300
#define LAPIC_ICR_HIGH      0x310
#define LAPIC_TIMER_LVT     0x320
#define LAPIC_LINT0_LVT     0x350
#define LAPIC_LINT1_LVT     0x360
#define LAPIC_ERROR_LVT     0x370
#define LAPIC_TIMER_ICR     0x380
#define LAPIC_TIMER_CCR     0x390
#define LAPIC_TIMER_DCR     0x3E0

/* ============================================================================
 * APIC Constants
 * ============================================================================ */

#define LAPIC_DEFAULT_BASE  0xFEE00000
#define LAPIC_SVR_ENABLE    BIT(8)

/* ICR bits */
#define ICR_FIXED           (0 << 8)
#define ICR_INIT            (5 << 8)
#define ICR_STARTUP         (6 << 8)
#define ICR_PHYSICAL        (0 << 11)
#define ICR_ASSERT          (1 << 14)
#define ICR_EDGE            (0 << 15)
#define ICR_LEVEL           (1 << 15)
#define ICR_NO_SHORTHAND    (0 << 18)
#define ICR_ALL_EXCL        (3 << 18)

/* LVT bits */
#define LVT_MASKED          BIT(16)
#define TIMER_PERIODIC      (1 << 17)
#define TIMER_DIV_16        0x03

/* I/O APIC */
#define IOAPIC_DEFAULT_BASE 0xFEC00000
#define IOAPIC_REGSEL       0x00
#define IOAPIC_WINDOW       0x10
#define IOAPIC_ID           0x00
#define IOAPIC_VER          0x01
#define IOAPIC_REDTBL       0x10
#define IOAPIC_MASKED       BIT(16)

/* ============================================================================
 * API Functions
 * ============================================================================ */

void lapic_init(void);
void lapic_enable(void);
uint32_t lapic_get_id(void);
void lapic_eoi(void);
void lapic_send_ipi(uint32_t dest, uint32_t vector);
void lapic_send_ipi_all(uint32_t vector, bool include_self);
void lapic_send_init(uint32_t dest);
void lapic_send_startup(uint32_t dest, uint8_t vector);
void lapic_timer_init(uint8_t vector, bool periodic);
void lapic_timer_set(uint32_t count);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);

void ioapic_init(void);
void ioapic_set_irq(uint8_t irq, uint8_t vector, uint8_t dest, uint32_t flags);
void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);

#endif /* _PUREVISOR_APIC_H */
