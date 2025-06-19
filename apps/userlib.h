/* apps/userlib.h - простая библиотека вывода для ring 3 (user-mode)
 * Реализует минимальные syscalls write/exit и printf()
 */
#ifndef USERLIB_H
#define USERLIB_H

#include <stdint.h>
#include <stdarg.h>

/* Номера системных вызовов должны совпадать с enum в include/syscall.h */
enum {
    SYS_write = 0,
    SYS_exit  = 1,
    SYS_getch = 2,
};

/* Обёртка вокруг int 0x80 (x86, 32-бит) */
static inline int __attribute__((always_inline))
_syscall3(int num, uint32_t a, uint32_t b, uint32_t c)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a" (ret)
                                   : "a" (num), "b" (a), "c" (b), "d" (c)
                                   : "memory");
    return ret;
}

static inline void sys_write(const char *s)
{
    _syscall3(SYS_write, (uint32_t)s, 0, 0);
}

static inline void sys_exit(int status)
{
    _syscall3(SYS_exit, (uint32_t)status, 0, 0);
    for (;;); /* если kernel не вернул */
}

static inline int getchar(void)
{
    return _syscall3(SYS_getch, 0, 0, 0);
}

/* ---------------- Простейшие функции работы со строками ---------------- */
static inline int strlen(const char *s)
{
    int i = 0;
    while (s[i]) ++i;
    return i;
}

static inline void reverse(char *str, int len)
{
    for (int i = 0, j = len - 1; i < j; ++i, --j) {
        char t = str[i];
        str[i] = str[j];
        str[j] = t;
    }
}

static inline void itoa_unsigned(uint32_t value, char *buf, int base)
{
    int i = 0;
    if (value == 0) {
        buf[i++] = '0';
        buf[i] = 0;
        return;
    }
    while (value) {
        uint32_t digit = value % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + (digit - 10));
        value /= base;
    }
    buf[i] = 0;
    reverse(buf, i);
}

static inline void itoa_signed(int32_t value, char *buf, int base)
{
    if (value < 0 && base == 10) {
        *buf++ = '-';
        itoa_unsigned((uint32_t)(-value), buf, base);
    } else {
        itoa_unsigned((uint32_t)value, buf, base);
    }
}

/* ---------------- Минимальная реализация vsprintf ---------------- */
static inline int vsprintf(char *out, const char *fmt, va_list ap)
{
    char *start = out;
    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') {
            *out++ = *p;
            continue;
        }
        ++p; /* пропускаем '%' */
        switch (*p) {
            case 'c': {
                int c = va_arg(ap, int);
                *out++ = (char)c;
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char*);
                while (*s) *out++ = *s++;
                break;
            }
            case 'd': {
                int32_t v = va_arg(ap, int32_t);
                char tmp[32];
                itoa_signed(v, tmp, 10);
                for (char *t = tmp; *t; ++t) *out++ = *t;
                break;
            }
            case 'u': {
                uint32_t v = va_arg(ap, uint32_t);
                char tmp[32];
                itoa_unsigned(v, tmp, 10);
                for (char *t = tmp; *t; ++t) *out++ = *t;
                break;
            }
            case 'x': {
                uint32_t v = va_arg(ap, uint32_t);
                char tmp[32];
                itoa_unsigned(v, tmp, 16);
                for (char *t = tmp; *t; ++t) *out++ = *t;
                break;
            }
            case '%': {
                *out++ = '%';
                break;
            }
            default: /* неизвестный спецификатор */
                *out++ = '%';
                *out++ = *p;
                break;
        }
    }
    *out = 0;
    return out - start;
}

/* ---------------- Публичный printf ---------------- */
static inline int printf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int len = vsprintf(buf, fmt, ap);
    va_end(ap);
    sys_write(buf);
    return len;
}

/* Обратная совместимость: kprintf идентичен printf */
static inline int kprintf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int len = vsprintf(buf, fmt, ap);
    va_end(ap);
    sys_write(buf);
    return len;
}

#endif /* USERLIB_H */ 