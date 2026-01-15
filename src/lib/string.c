/*
 * PureVisor - String/Memory Functions Implementation
 * 
 * Minimal libc replacement - no external dependencies
 */

#include <lib/types.h>
#include <lib/string.h>

/* ============================================================================
 * Memory Functions
 * ============================================================================ */

void *memset(void *dest, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    uint8_t val = (uint8_t)c;
    
    /* Optimize for aligned, large fills */
    if (n >= 8 && ((uintptr_t)d & 7) == 0) {
        uint64_t val64 = val;
        val64 |= val64 << 8;
        val64 |= val64 << 16;
        val64 |= val64 << 32;
        
        while (n >= 8) {
            *(uint64_t *)d = val64;
            d += 8;
            n -= 8;
        }
    }
    
    while (n--) {
        *d++ = val;
    }
    
    return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    /* Optimize for aligned copies */
    if (n >= 8 && ((uintptr_t)d & 7) == 0 && ((uintptr_t)s & 7) == 0) {
        while (n >= 8) {
            *(uint64_t *)d = *(const uint64_t *)s;
            d += 8;
            s += 8;
            n -= 8;
        }
    }
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    if (d < s) {
        /* Forward copy */
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* Backward copy (for overlapping regions) */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const uint8_t *p = (const uint8_t *)s;
    uint8_t val = (uint8_t)c;
    
    while (n--) {
        if (*p == val) {
            return (void *)p;
        }
        p++;
    }
    
    return NULL;
}

/* ============================================================================
 * String Functions
 * ============================================================================ */

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) {
        p++;
    }
    return p - s;
}

size_t strnlen(const char *s, size_t maxlen)
{
    const char *p = s;
    while (maxlen-- && *p) {
        p++;
    }
    return p - s;
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    
    while (n && (*d++ = *src++)) {
        n--;
    }
    
    /* Pad with nulls if src was shorter than n */
    while (n--) {
        *d++ = '\0';
    }
    
    return dest;
}

char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d) {
        d++;
    }
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d) {
        d++;
    }
    while (n-- && (*d = *src++)) {
        d++;
    }
    *d = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    if (!n) {
        return 0;
    }
    
    while (--n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;
    
    while (*s) {
        if (*s == ch) {
            return (char *)s;
        }
        s++;
    }
    
    /* Also match null terminator */
    return (ch == '\0') ? (char *)s : NULL;
}

char *strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *last = NULL;
    
    while (*s) {
        if (*s == ch) {
            last = s;
        }
        s++;
    }
    
    return (ch == '\0') ? (char *)s : (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle) {
        return (char *)haystack;
    }
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        
        if (!*n) {
            return (char *)haystack;
        }
        
        haystack++;
    }
    
    return NULL;
}

/* ============================================================================
 * Number Conversion Functions
 * ============================================================================ */

static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

char *utoa(uint64_t value, char *str, int base)
{
    char *p = str;
    char *p1, *p2;
    char tmp;
    
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
    /* Generate digits in reverse order */
    do {
        *p++ = digits[value % base];
        value /= base;
    } while (value);
    
    *p = '\0';
    
    /* Reverse the string */
    p1 = str;
    p2 = p - 1;
    while (p1 < p2) {
        tmp = *p1;
        *p1++ = *p2;
        *p2-- = tmp;
    }
    
    return str;
}

char *itoa(int64_t value, char *str, int base)
{
    if (base == 10 && value < 0) {
        *str = '-';
        utoa((uint64_t)(-value), str + 1, base);
        return str;
    }
    
    return utoa((uint64_t)value, str, base);
}

int64_t atoi(const char *str)
{
    int64_t result = 0;
    int negative = 0;
    
    /* Skip whitespace */
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    /* Handle sign */
    if (*str == '-') {
        negative = 1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    /* Convert digits */
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return negative ? -result : result;
}

uint64_t atou(const char *str)
{
    uint64_t result = 0;
    
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result;
}

uint64_t strtoul(const char *str, char **endptr, int base)
{
    uint64_t result = 0;
    const char *p = str;
    
    /* Skip whitespace */
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    /* Handle optional + */
    if (*p == '+') {
        p++;
    }
    
    /* Auto-detect base */
    if (base == 0) {
        if (*p == '0') {
            if (p[1] == 'x' || p[1] == 'X') {
                base = 16;
                p += 2;
            } else {
                base = 8;
                p++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }
    
    /* Convert */
    while (*p) {
        int digit;
        
        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        p++;
    }
    
    if (endptr) {
        *endptr = (char *)p;
    }
    
    return result;
}

/* ============================================================================
 * Formatting Functions
 * ============================================================================ */

#include <stdarg.h>

/* Helper to output a character with bounds checking */
static int put_char(char *buf, size_t *pos, size_t size, char c)
{
    if (*pos < size - 1) {
        buf[*pos] = c;
    }
    (*pos)++;
    return 1;
}

/* Helper to output a string */
static int put_string(char *buf, size_t *pos, size_t size, const char *s,
                      int width, int left_align, char pad)
{
    int len = strlen(s);
    int padding = (width > len) ? width - len : 0;
    int written = 0;
    
    if (!left_align) {
        while (padding-- > 0) {
            written += put_char(buf, pos, size, pad);
        }
    }
    
    while (*s) {
        written += put_char(buf, pos, size, *s++);
    }
    
    if (left_align) {
        while (padding-- > 0) {
            written += put_char(buf, pos, size, ' ');
        }
    }
    
    return written;
}

/* Helper to output an integer */
static int put_int(char *buf, size_t *pos, size_t size, uint64_t value,
                   int base, int is_signed, int width, int left_align,
                   char pad, int uppercase)
{
    char numbuf[65];
    char *p = numbuf + sizeof(numbuf) - 1;
    int negative = 0;
    const char *digits_str = uppercase ? 
        "0123456789ABCDEF" : "0123456789abcdef";
    
    *p = '\0';
    
    if (is_signed && (int64_t)value < 0) {
        negative = 1;
        value = (uint64_t)(-(int64_t)value);
    }
    
    if (value == 0) {
        *--p = '0';
    } else {
        while (value) {
            *--p = digits_str[value % base];
            value /= base;
        }
    }
    
    if (negative) {
        *--p = '-';
    }
    
    return put_string(buf, pos, size, p, width, left_align, pad);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    size_t pos = 0;
    
    if (size == 0) {
        return 0;
    }
    
    while (*format) {
        if (*format != '%') {
            put_char(str, &pos, size, *format++);
            continue;
        }
        
        format++;
        
        /* Parse flags */
        int left_align = 0;
        char pad = ' ';
        
        while (*format == '-' || *format == '0') {
            if (*format == '-') left_align = 1;
            if (*format == '0' && !left_align) pad = '0';
            format++;
        }
        
        /* Parse width */
        int width = 0;
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format++ - '0');
        }
        
        /* Parse length modifier */
        int is_long = 0;
        int is_longlong = 0;
        
        if (*format == 'l') {
            format++;
            is_long = 1;
            if (*format == 'l') {
                format++;
                is_longlong = 1;
            }
        } else if (*format == 'z') {
            format++;
            is_longlong = 1;  /* size_t is 64-bit on x86_64 */
        }
        
        /* Parse conversion specifier */
        switch (*format) {
        case 'd':
        case 'i': {
            int64_t val;
            if (is_longlong) val = va_arg(ap, int64_t);
            else if (is_long) val = va_arg(ap, long);
            else val = va_arg(ap, int);
            put_int(str, &pos, size, val, 10, 1, width, left_align, pad, 0);
            break;
        }
        case 'u': {
            uint64_t val;
            if (is_longlong) val = va_arg(ap, uint64_t);
            else if (is_long) val = va_arg(ap, unsigned long);
            else val = va_arg(ap, unsigned int);
            put_int(str, &pos, size, val, 10, 0, width, left_align, pad, 0);
            break;
        }
        case 'x': {
            uint64_t val;
            if (is_longlong) val = va_arg(ap, uint64_t);
            else if (is_long) val = va_arg(ap, unsigned long);
            else val = va_arg(ap, unsigned int);
            put_int(str, &pos, size, val, 16, 0, width, left_align, pad, 0);
            break;
        }
        case 'X': {
            uint64_t val;
            if (is_longlong) val = va_arg(ap, uint64_t);
            else if (is_long) val = va_arg(ap, unsigned long);
            else val = va_arg(ap, unsigned int);
            put_int(str, &pos, size, val, 16, 0, width, left_align, pad, 1);
            break;
        }
        case 'p': {
            void *ptr = va_arg(ap, void *);
            put_string(str, &pos, size, "0x", 0, 0, ' ');
            put_int(str, &pos, size, (uint64_t)ptr, 16, 0, 16, 0, '0', 0);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            put_string(str, &pos, size, s, width, left_align, ' ');
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            put_char(str, &pos, size, c);
            break;
        }
        case '%':
            put_char(str, &pos, size, '%');
            break;
        default:
            put_char(str, &pos, size, '%');
            put_char(str, &pos, size, *format);
            break;
        }
        
        format++;
    }
    
    /* Null terminate */
    if (pos < size) {
        str[pos] = '\0';
    } else if (size > 0) {
        str[size - 1] = '\0';
    }
    
    return pos;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int ret;
    
    va_start(ap, format);
    ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    
    return ret;
}
