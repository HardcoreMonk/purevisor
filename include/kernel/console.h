/*
 * PureVisor - Console Output Header
 * 
 * Serial port and VGA text mode output
 */

#ifndef _PUREVISOR_CONSOLE_H
#define _PUREVISOR_CONSOLE_H

#include <lib/types.h>

/* ============================================================================
 * Serial Port Constants
 * ============================================================================ */

#define COM1_PORT           0x3F8
#define COM2_PORT           0x2F8
#define COM3_PORT           0x3E8
#define COM4_PORT           0x2E8

/* Serial port registers (offset from base) */
#define SERIAL_DATA         0   /* Data Register (R/W) */
#define SERIAL_IER          1   /* Interrupt Enable Register */
#define SERIAL_FCR          2   /* FIFO Control Register (Write) */
#define SERIAL_IIR          2   /* Interrupt ID Register (Read) */
#define SERIAL_LCR          3   /* Line Control Register */
#define SERIAL_MCR          4   /* Modem Control Register */
#define SERIAL_LSR          5   /* Line Status Register */
#define SERIAL_MSR          6   /* Modem Status Register */

/* Serial port divisor (when DLAB=1) */
#define SERIAL_DLL          0   /* Divisor Latch Low */
#define SERIAL_DLH          1   /* Divisor Latch High */

/* Line Status Register bits */
#define SERIAL_LSR_DR       BIT(0)  /* Data Ready */
#define SERIAL_LSR_THRE     BIT(5)  /* Transmitter Holding Register Empty */

/* Line Control Register bits */
#define SERIAL_LCR_DLAB     BIT(7)  /* Divisor Latch Access Bit */
#define SERIAL_LCR_8N1      0x03    /* 8 bits, no parity, 1 stop bit */

/* FIFO Control Register bits */
#define SERIAL_FCR_ENABLE   BIT(0)  /* Enable FIFOs */
#define SERIAL_FCR_CLEAR_RX BIT(1)  /* Clear receive FIFO */
#define SERIAL_FCR_CLEAR_TX BIT(2)  /* Clear transmit FIFO */
#define SERIAL_FCR_14_BYTE  0xC0    /* 14-byte FIFO trigger level */

/* Modem Control Register bits */
#define SERIAL_MCR_DTR      BIT(0)  /* Data Terminal Ready */
#define SERIAL_MCR_RTS      BIT(1)  /* Request to Send */
#define SERIAL_MCR_OUT2     BIT(3)  /* Enable IRQ */

/* ============================================================================
 * VGA Text Mode Constants
 * ============================================================================ */

#define VGA_TEXT_BUFFER     0xB8000
#define VGA_WIDTH           80
#define VGA_HEIGHT          25

/* VGA colors */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

#define VGA_ENTRY_COLOR(fg, bg)     ((fg) | ((bg) << 4))
#define VGA_ENTRY(c, color)         ((uint16_t)(c) | ((uint16_t)(color) << 8))

/* ============================================================================
 * Console API
 * ============================================================================ */

/**
 * console_init - Initialize console output
 * 
 * Initializes both serial port (COM1) and VGA text mode
 */
void console_init(void);

/**
 * console_putchar - Output a single character
 * @c: Character to output
 */
void console_putchar(char c);

/**
 * console_puts - Output a string
 * @s: Null-terminated string
 */
void console_puts(const char *s);

/**
 * console_write - Output a buffer
 * @buf: Buffer to output
 * @len: Number of bytes
 */
void console_write(const char *buf, size_t len);

/**
 * kprintf - Kernel printf
 * @fmt: Format string
 * @...: Arguments
 * 
 * Returns number of characters written
 */
int kprintf(const char *fmt, ...);

/* ============================================================================
 * Serial Port API
 * ============================================================================ */

/**
 * serial_init - Initialize serial port
 * @port: Port base address (COM1_PORT, etc.)
 * @baud: Baud rate (default 115200)
 */
void serial_init(uint16_t port, uint32_t baud);

/**
 * serial_putchar - Output character to serial port
 * @port: Port base address
 * @c: Character to output
 */
void serial_putchar(uint16_t port, char c);

/**
 * serial_puts - Output string to serial port
 * @port: Port base address
 * @s: Null-terminated string
 */
void serial_puts(uint16_t port, const char *s);

/**
 * serial_getchar - Read character from serial port (blocking)
 * @port: Port base address
 * 
 * Returns character read
 */
char serial_getchar(uint16_t port);

/**
 * serial_can_read - Check if data is available
 * @port: Port base address
 * 
 * Returns true if data is available
 */
bool serial_can_read(uint16_t port);

/* ============================================================================
 * VGA Text Mode API
 * ============================================================================ */

/**
 * vga_init - Initialize VGA text mode
 */
void vga_init(void);

/**
 * vga_clear - Clear the screen
 */
void vga_clear(void);

/**
 * vga_setcolor - Set text color
 * @fg: Foreground color
 * @bg: Background color
 */
void vga_setcolor(vga_color_t fg, vga_color_t bg);

/**
 * vga_putchar - Output character at current cursor
 * @c: Character to output
 */
void vga_putchar(char c);

/**
 * vga_puts - Output string at current cursor
 * @s: Null-terminated string
 */
void vga_puts(const char *s);

/**
 * vga_setcursor - Set cursor position
 * @x: Column (0-79)
 * @y: Row (0-24)
 */
void vga_setcursor(int x, int y);

/**
 * vga_scroll - Scroll screen up by one line
 */
void vga_scroll(void);

/* ============================================================================
 * Debug Output Macros
 * ============================================================================ */

/* Log levels */
typedef enum {
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARN    = 2,
    LOG_ERROR   = 3,
    LOG_FATAL   = 4,
} log_level_t;

#define pr_debug(fmt, ...) kprintf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define pr_info(fmt, ...)  kprintf("[INFO]  " fmt "\n", ##__VA_ARGS__)
#define pr_warn(fmt, ...)  kprintf("[WARN]  " fmt "\n", ##__VA_ARGS__)
#define pr_error(fmt, ...) kprintf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define pr_fatal(fmt, ...) kprintf("[FATAL] " fmt "\n", ##__VA_ARGS__)

/* Debug assertion */
#define ASSERT(cond) do { \
    if (!(cond)) { \
        pr_fatal("Assertion failed: %s at %s:%d", #cond, __FILE__, __LINE__); \
        while(1) hlt(); \
    } \
} while(0)

/* Panic - halt the system */
#define panic(fmt, ...) do { \
    pr_fatal("PANIC: " fmt, ##__VA_ARGS__); \
    pr_fatal("at %s:%d", __FILE__, __LINE__); \
    cli(); \
    while(1) hlt(); \
} while(0)

#endif /* _PUREVISOR_CONSOLE_H */
