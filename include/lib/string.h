/*
 * PureVisor - String/Memory Functions Header
 * 
 * Minimal libc replacement functions
 */

#ifndef _PUREVISOR_STRING_H
#define _PUREVISOR_STRING_H

#include <lib/types.h>

/* ============================================================================
 * Memory Functions
 * ============================================================================ */

/**
 * memset - Fill memory with a constant byte
 * @dest: Pointer to the destination memory
 * @c: Byte value to set
 * @n: Number of bytes to set
 * 
 * Returns pointer to dest
 */
void *memset(void *dest, int c, size_t n);

/**
 * memcpy - Copy memory area
 * @dest: Pointer to destination
 * @src: Pointer to source
 * @n: Number of bytes to copy
 * 
 * Returns pointer to dest
 * Note: Areas must not overlap (use memmove for overlapping)
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * memmove - Copy memory area (handles overlapping)
 * @dest: Pointer to destination
 * @src: Pointer to source
 * @n: Number of bytes to copy
 * 
 * Returns pointer to dest
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * memcmp - Compare memory areas
 * @s1: First memory area
 * @s2: Second memory area
 * @n: Number of bytes to compare
 * 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2
 */
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * memchr - Locate byte in memory
 * @s: Memory area to search
 * @c: Byte to find
 * @n: Maximum bytes to search
 * 
 * Returns pointer to byte or NULL if not found
 */
void *memchr(const void *s, int c, size_t n);

/* ============================================================================
 * String Functions
 * ============================================================================ */

/**
 * strlen - Calculate string length
 * @s: Null-terminated string
 * 
 * Returns number of characters (not including null terminator)
 */
size_t strlen(const char *s);

/**
 * strnlen - Calculate string length with limit
 * @s: String
 * @maxlen: Maximum length to check
 * 
 * Returns min(strlen(s), maxlen)
 */
size_t strnlen(const char *s, size_t maxlen);

/**
 * strcpy - Copy string
 * @dest: Destination buffer
 * @src: Source string
 * 
 * Returns pointer to dest
 */
char *strcpy(char *dest, const char *src);

/**
 * strncpy - Copy string with length limit
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum bytes to copy
 * 
 * Returns pointer to dest
 */
char *strncpy(char *dest, const char *src, size_t n);

/**
 * strcat - Concatenate strings
 * @dest: Destination string
 * @src: Source string to append
 * 
 * Returns pointer to dest
 */
char *strcat(char *dest, const char *src);

/**
 * strncat - Concatenate strings with length limit
 * @dest: Destination string
 * @src: Source string to append
 * @n: Maximum bytes from src
 * 
 * Returns pointer to dest
 */
char *strncat(char *dest, const char *src, size_t n);

/**
 * strcmp - Compare strings
 * @s1: First string
 * @s2: Second string
 * 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2
 */
int strcmp(const char *s1, const char *s2);

/**
 * strncmp - Compare strings with length limit
 * @s1: First string
 * @s2: Second string
 * @n: Maximum bytes to compare
 * 
 * Returns <0 if s1<s2, 0 if s1==s2, >0 if s1>s2
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * strchr - Find character in string
 * @s: String to search
 * @c: Character to find
 * 
 * Returns pointer to character or NULL
 */
char *strchr(const char *s, int c);

/**
 * strrchr - Find last occurrence of character
 * @s: String to search
 * @c: Character to find
 * 
 * Returns pointer to last occurrence or NULL
 */
char *strrchr(const char *s, int c);

/**
 * strstr - Find substring
 * @haystack: String to search in
 * @needle: Substring to find
 * 
 * Returns pointer to start of needle or NULL
 */
char *strstr(const char *haystack, const char *needle);

/* ============================================================================
 * Number Conversion Functions
 * ============================================================================ */

/**
 * itoa - Convert integer to string
 * @value: Integer value
 * @str: Output buffer
 * @base: Number base (2-36)
 * 
 * Returns pointer to str
 */
char *itoa(int64_t value, char *str, int base);

/**
 * utoa - Convert unsigned integer to string
 * @value: Unsigned integer value
 * @str: Output buffer
 * @base: Number base (2-36)
 * 
 * Returns pointer to str
 */
char *utoa(uint64_t value, char *str, int base);

/**
 * atoi - Convert string to integer
 * @str: String to convert
 * 
 * Returns integer value
 */
int64_t atoi(const char *str);

/**
 * atou - Convert string to unsigned integer
 * @str: String to convert
 * 
 * Returns unsigned integer value
 */
uint64_t atou(const char *str);

/**
 * strtoul - Convert string to unsigned long with base
 * @str: String to convert
 * @endptr: If non-NULL, stores pointer to first invalid character
 * @base: Number base (0 for auto-detect, 2-36)
 * 
 * Returns unsigned long value
 */
uint64_t strtoul(const char *str, char **endptr, int base);

/* ============================================================================
 * Formatting Functions
 * ============================================================================ */

/**
 * snprintf - Format string with length limit
 * @str: Output buffer
 * @size: Buffer size
 * @format: Format string
 * @...: Arguments
 * 
 * Returns number of characters that would be written (excluding null)
 */
int snprintf(char *str, size_t size, const char *format, ...);

/**
 * vsnprintf - Format string with va_list
 * @str: Output buffer
 * @size: Buffer size
 * @format: Format string
 * @ap: Argument list
 * 
 * Returns number of characters that would be written (excluding null)
 */
#include <stdarg.h>
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

#endif /* _PUREVISOR_STRING_H */
