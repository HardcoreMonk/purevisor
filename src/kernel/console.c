/*
 * PureVisor - Console Output Implementation
 * 
 * Serial port (COM1) and VGA text mode output
 */

#include <lib/types.h>
#include <lib/string.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * VGA Text Mode State
 * ============================================================================ */

static volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_TEXT_BUFFER;
static int vga_row = 0;
static int vga_col = 0;
static uint8_t vga_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

/* ============================================================================
 * Serial Port Implementation
 * ============================================================================ */

void serial_init(uint16_t port, uint32_t baud)
{
    uint16_t divisor = 115200 / baud;
    
    /* Disable interrupts */
    outb(port + SERIAL_IER, 0x00);
    
    /* Enable DLAB (set baud rate divisor) */
    outb(port + SERIAL_LCR, SERIAL_LCR_DLAB);
    
    /* Set divisor (low byte) */
    outb(port + SERIAL_DLL, divisor & 0xFF);
    
    /* Set divisor (high byte) */
    outb(port + SERIAL_DLH, (divisor >> 8) & 0xFF);
    
    /* 8 bits, no parity, one stop bit */
    outb(port + SERIAL_LCR, SERIAL_LCR_8N1);
    
    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(port + SERIAL_FCR, SERIAL_FCR_ENABLE | SERIAL_FCR_CLEAR_RX | 
                            SERIAL_FCR_CLEAR_TX | SERIAL_FCR_14_BYTE);
    
    /* Enable IRQs, RTS/DSR set */
    outb(port + SERIAL_MCR, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
}

static bool serial_is_transmit_empty(uint16_t port)
{
    return (inb(port + SERIAL_LSR) & SERIAL_LSR_THRE) != 0;
}

void serial_putchar(uint16_t port, char c)
{
    /* Wait for transmit buffer to be empty */
    while (!serial_is_transmit_empty(port)) {
        pause();
    }
    
    outb(port + SERIAL_DATA, c);
}

void serial_puts(uint16_t port, const char *s)
{
    while (*s) {
        if (*s == '\n') {
            serial_putchar(port, '\r');
        }
        serial_putchar(port, *s++);
    }
}

bool serial_can_read(uint16_t port)
{
    return (inb(port + SERIAL_LSR) & SERIAL_LSR_DR) != 0;
}

char serial_getchar(uint16_t port)
{
    while (!serial_can_read(port)) {
        pause();
    }
    return inb(port + SERIAL_DATA);
}

/* ============================================================================
 * VGA Text Mode Implementation
 * ============================================================================ */

void vga_init(void)
{
    vga_row = 0;
    vga_col = 0;
    vga_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

void vga_clear(void)
{
    uint16_t blank = VGA_ENTRY(' ', vga_color);
    
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    
    vga_row = 0;
    vga_col = 0;
}

void vga_setcolor(vga_color_t fg, vga_color_t bg)
{
    vga_color = VGA_ENTRY_COLOR(fg, bg);
}

void vga_scroll(void)
{
    /* Move all lines up by one */
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    /* Clear the last line */
    uint16_t blank = VGA_ENTRY(' ', vga_color);
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
}

void vga_setcursor(int x, int y)
{
    vga_col = x;
    vga_row = y;
}

void vga_putchar(char c)
{
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buffer[vga_row * VGA_WIDTH + vga_col] = VGA_ENTRY(' ', vga_color);
        }
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = VGA_ENTRY(c, vga_color);
        vga_col++;
    }
    
    /* Handle line wrap */
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
    }
    
    /* Handle scroll */
    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_puts(const char *s)
{
    while (*s) {
        vga_putchar(*s++);
    }
}

/* ============================================================================
 * Unified Console Implementation
 * ============================================================================ */

static bool console_initialized = false;

void console_init(void)
{
    /* Initialize serial port (COM1 at 115200 baud) */
    serial_init(COM1_PORT, 115200);
    
    /* Initialize VGA text mode */
    vga_init();
    
    console_initialized = true;
}

void console_putchar(char c)
{
    /* Output to serial */
    if (c == '\n') {
        serial_putchar(COM1_PORT, '\r');
    }
    serial_putchar(COM1_PORT, c);
    
    /* Output to VGA */
    vga_putchar(c);
}

void console_puts(const char *s)
{
    while (*s) {
        console_putchar(*s++);
    }
}

void console_write(const char *buf, size_t len)
{
    while (len--) {
        console_putchar(*buf++);
    }
}

/* ============================================================================
 * Kernel Printf
 * ============================================================================ */

#include <stdarg.h>

#define KPRINTF_BUFFER_SIZE 1024

int kprintf(const char *fmt, ...)
{
    char buf[KPRINTF_BUFFER_SIZE];
    va_list ap;
    int len;
    
    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    console_puts(buf);
    
    return len;
}
