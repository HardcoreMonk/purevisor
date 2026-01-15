/*
 * PureVisor - Interrupt Handling Implementation
 * 
 * IDT setup and exception/IRQ handlers
 */

#include <lib/types.h>
#include <lib/string.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * Interrupt Frame Structure (matches assembly pushes)
 * ============================================================================ */

typedef struct PACKED {
    /* Pushed by our handler */
    uint64_t es;
    uint64_t ds;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    /* Pushed by our stub */
    uint64_t int_no;
    uint64_t error_code;
    
    /* Pushed by CPU */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

/* ============================================================================
 * IDT Data
 * ============================================================================ */

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES] ALIGNED(16);
static idt_ptr_t idt_ptr;

/* ISR stub table (defined in idt.asm) */
extern uint64_t isr_stub_table[48];

/* Assembly function to load IDT */
extern void idt_load(idt_ptr_t *ptr);

/* ============================================================================
 * Exception Names
 * ============================================================================ */

static const char *exception_names[] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization Exception",
    "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security Exception",
    "Reserved"
};

/* ============================================================================
 * PIC (Programmable Interrupt Controller) Constants
 * ============================================================================ */

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define PIC_EOI         0x20    /* End of Interrupt */

#define ICW1_ICW4       0x01    /* ICW4 needed */
#define ICW1_SINGLE     0x02    /* Single mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 */
#define ICW1_LEVEL      0x08    /* Level triggered mode */
#define ICW1_INIT       0x10    /* Initialization */

#define ICW4_8086       0x01    /* 8086/88 mode */
#define ICW4_AUTO       0x02    /* Auto EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered slave */
#define ICW4_BUF_MASTER 0x0C    /* Buffered master */
#define ICW4_SFNM       0x10    /* Special fully nested */

/* ============================================================================
 * IDT Entry Setup
 * ============================================================================ */

static void idt_set_gate(int n, uint64_t handler, uint16_t selector, 
                         uint8_t ist, uint8_t type_attr)
{
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector = selector;
    idt[n].ist = ist;
    idt[n].type_attr = type_attr;
    idt[n].offset_middle = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].reserved = 0;
}

/* ============================================================================
 * PIC Initialization
 * ============================================================================ */

static void pic_remap(int offset1, int offset2)
{
    uint8_t mask1, mask2;
    
    /* Save masks */
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);
    
    /* Start initialization sequence */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    
    /* ICW2: Vector offset */
    outb(PIC1_DATA, offset1);       /* Master PIC vector offset */
    outb(PIC2_DATA, offset2);       /* Slave PIC vector offset */
    
    /* ICW3: Master/Slave wiring */
    outb(PIC1_DATA, 4);             /* Tell Master PIC slave is at IRQ2 */
    outb(PIC2_DATA, 2);             /* Tell Slave PIC its cascade identity */
    
    /* ICW4: Mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    
    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

static void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

static void pic_disable(void)
{
    /* Mask all interrupts */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ============================================================================
 * IDT Initialization
 * ============================================================================ */

void idt_init(void)
{
    /* Clear IDT */
    memset(idt, 0, sizeof(idt));
    
    /* Set up exception handlers (0-31) */
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEL,
                     0, IDT_ATTR_PRESENT | IDT_TYPE_INTERRUPT);
    }
    
    /* Remap PIC to vectors 32-47 */
    pic_remap(32, 40);
    
    /* Set up IRQ handlers (32-47) */
    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, isr_stub_table[32 + i], GDT_KERNEL_CODE_SEL,
                     0, IDT_ATTR_PRESENT | IDT_TYPE_INTERRUPT);
    }
    
    /* Mask all IRQs initially */
    pic_disable();
    
    /* Set up IDT pointer */
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)idt;
    
    /* Load IDT */
    idt_load(&idt_ptr);
    
    pr_info("IDT initialized with %d entries", IDT_ENTRIES);
}

/* ============================================================================
 * Exception Handler (called from assembly)
 * ============================================================================ */

void exception_handler(interrupt_frame_t *frame)
{
    int int_no = frame->int_no;
    
    kprintf("\n");
    kprintf("=== EXCEPTION %d: %s ===\n", 
            int_no, 
            int_no < 32 ? exception_names[int_no] : "Unknown");
    kprintf("Error Code: 0x%016llx\n", frame->error_code);
    kprintf("\n");
    kprintf("RAX: 0x%016llx  RBX: 0x%016llx\n", frame->rax, frame->rbx);
    kprintf("RCX: 0x%016llx  RDX: 0x%016llx\n", frame->rcx, frame->rdx);
    kprintf("RSI: 0x%016llx  RDI: 0x%016llx\n", frame->rsi, frame->rdi);
    kprintf("RBP: 0x%016llx  RSP: 0x%016llx\n", frame->rbp, frame->rsp);
    kprintf("R8:  0x%016llx  R9:  0x%016llx\n", frame->r8, frame->r9);
    kprintf("R10: 0x%016llx  R11: 0x%016llx\n", frame->r10, frame->r11);
    kprintf("R12: 0x%016llx  R13: 0x%016llx\n", frame->r12, frame->r13);
    kprintf("R14: 0x%016llx  R15: 0x%016llx\n", frame->r14, frame->r15);
    kprintf("\n");
    kprintf("RIP: 0x%016llx  CS:  0x%04llx\n", frame->rip, frame->cs);
    kprintf("RFLAGS: 0x%016llx\n", frame->rflags);
    kprintf("SS:  0x%04llx\n", frame->ss);
    
    /* Special handling for Page Fault */
    if (int_no == 14) {
        uint64_t cr2 = read_cr2();
        kprintf("\nPage Fault Address (CR2): 0x%016llx\n", cr2);
        
        kprintf("Fault reason: ");
        if (!(frame->error_code & 0x1)) kprintf("Page not present. ");
        if (frame->error_code & 0x2) kprintf("Write access. ");
        if (frame->error_code & 0x4) kprintf("User mode. ");
        if (frame->error_code & 0x8) kprintf("Reserved bit set. ");
        if (frame->error_code & 0x10) kprintf("Instruction fetch. ");
        kprintf("\n");
    }
    
    kprintf("\nSystem halted.\n");
    
    /* Halt the system */
    cli();
    while (1) {
        hlt();
    }
}

/* ============================================================================
 * IRQ Handler (called from assembly)
 * ============================================================================ */

/* IRQ handler function pointers */
typedef void (*irq_handler_fn)(interrupt_frame_t *frame);
static irq_handler_fn irq_handlers[16] = {0};

void irq_register_handler(int irq, irq_handler_fn handler)
{
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}

void irq_handler(interrupt_frame_t *frame)
{
    int irq = frame->int_no - 32;
    
    /* Call registered handler if any */
    if (irq >= 0 && irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq](frame);
    }
    
    /* Send EOI to PIC */
    pic_send_eoi(irq);
}

/* ============================================================================
 * IRQ Enable/Disable
 * ============================================================================ */

void irq_enable(int irq)
{
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void irq_disable(int irq)
{
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}
