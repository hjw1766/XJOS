/* vsprintf.c -- Lars Wirzenius & Linus Torvalds, optimized inline assembly version */

#include <xjos/stdarg.h>
#include <xjos/string.h>
#include <xjos/assert.h>

#define ZEROPAD 1   // Pad with zero
#define SIGN 2      // Unsigned/signed long
#define PLUS 4      // Show plus sign
#define SPACE 8     // If plus, put a space
#define LEFT 16     // Left justified
#define SPECIAL 32  // 0x or 0
#define SMALL 64    // Use lowercase letters

#define is_digit(c) ((c) >= '0' && (c) <= '9')

static int skip_atoi(const char **s) {
    int i = 0;
    while (is_digit(**s)) {
        i = (i << 3) + (i << 1) + (*((*s)++) - '0'); // i*10 + digit
    }
    return i;
}

static char *number(char *str, unsigned long num, int base, int size, int precision, int flags) {
    char c, sign, tmp[36];
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;

    if (flags & SMALL) {
        digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    }

    if (flags & LEFT) {
        flags &= ~ZEROPAD;
    }

    if (base < 2 || base > 36) {
        return 0;
    }

    c = (flags & ZEROPAD) ? '0' : ' ';

    if (flags & SIGN && (signed long)num < 0) {
        sign = '-';
        num = -(signed long)num;
    } else {
        if (flags & PLUS) {
            sign = '+';
        } else if (flags & SPACE) {
            sign = ' ';
        } else {
            sign = 0;
        }
    }

    if (sign) {
        size--;
    }

    if (flags & SPECIAL) {
        if (base == 16) {
            size -= 2;
        } else if (base == 8) {
            size--;
        }
    }

    i = 0;
    if (num == 0) {
        tmp[i++] = '0';
    } else {
        char *p = tmp;
        __asm__ __volatile__ (
            "1: \n\t"
            "xorl %%edx, %%edx        \n\t"   // EDX = 0
            "divl %[base]             \n\t"   // EAX = EAX / base, EDX = remainder
            "movb (%[digits],%%edx,1), %%bl \n\t" // BL = digits[EDX]
            "movb %%bl,(%[p])         \n\t"   // *p = BL
            "leal 1(%[p]),%[p]        \n\t"   // p++
            "testl %%eax, %%eax       \n\t"   // quotient == 0 ?
            "jne 1b                   \n\t"   // loop if not
            : [p] "+r"(p), "+a"(num)
            : [base] "r"(base), [digits] "r"(digits)
            : "edx", "ebx", "memory", "cc"
        );
        i = p - tmp;
    }


    if (i > precision) {
        precision = i;
    }

    size -= precision;

    if (!(flags & (ZEROPAD + LEFT))) {
        while (size-- > 0) {
            *str++ = ' ';
        }
    }

    if (sign) {
        *str++ = sign;
    }

    if (flags & SPECIAL) {
        if (base == 8) {
            *str++ = '0';
        } else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33]; // 'X' or 'x'
        }
    }

    if (!(flags & LEFT)) {
        while (size-- > 0) {
            *str++ = c;
        }
    }

    while (i < precision--) {
        *str++ = '0';
    }

    while (i-- > 0) {
        *str++ = tmp[i];
    }

    while (size-- > 0) {
        *str++ = ' ';
    }

    return str;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    int len;
    int i;
    char *str;
    char *s;
    int *ip;
    int flags;
    int field_width;
    int precision;
    int qualifier;

    for (str = buf; *fmt; ++fmt) {
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }

        flags = 0;
    repeat:
        ++fmt;
        switch (*fmt) {
        case '-': flags |= LEFT; goto repeat;
        case '+': flags |= PLUS; goto repeat;
        case ' ': flags |= SPACE; goto repeat;
        case '#': flags |= SPECIAL; goto repeat;
        case '0': flags |= ZEROPAD; goto repeat;
        }

        field_width = -1;
        if (is_digit(*fmt)) {
            field_width = skip_atoi(&fmt);
        } else if (*fmt == '*') {
            ++fmt;
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (is_digit(*fmt)) {
                precision = skip_atoi(&fmt);
            } else if (*fmt == '*') {
                precision = va_arg(args, int);
            }
            if (precision < 0) {
                precision = 0;
            }
        }

        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT)) {
                while (--field_width > 0) {
                    *str++ = ' ';
                }
            }
            *str++ = (unsigned char)va_arg(args, int);
            while (--field_width > 0) {
                *str++ = ' ';
            }
            break;

        case 's':
            s = va_arg(args, char *);
            if (!s) {
                s = "<NULL>";
            }
            len = strlen(s);
            if (precision >= 0 && len > precision) {
                len = precision;
            }
            if (!(flags & LEFT)) {
                while (len < field_width--) {
                    *str++ = ' ';
                }
            }
            for (i = 0; i < len; ++i) {
                *str++ = *s++;
            }
            while (len < field_width--) {
                *str++ = ' ';
            }
            break;

        case 'o':
            str = number(str, va_arg(args, unsigned long), 8,
                         field_width, precision, flags);
            break;

        case 'p':
            if (field_width == -1) {
                field_width = 8;
                flags |= ZEROPAD;
            }
            str = number(str, (unsigned long)va_arg(args, void *), 16,
                         field_width, precision, flags);
            break;

        case 'x':
            flags |= SMALL;
        case 'X':
            str = number(str, va_arg(args, unsigned long), 16,
                         field_width, precision, flags);
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            str = number(str, va_arg(args, unsigned long), 10,
                         field_width, precision, flags);
            break;

        case 'n':
            ip = va_arg(args, int *);
            *ip = (str - buf);
            break;

        default:
            if (*fmt != '%') {
                *str++ = '%';
            }
            if (*fmt) {
                *str++ = *fmt;
            } else {
                --fmt;
            }
            break;
        }
    }
    *str = '\0';

    assert((str - buf) < 1024);
    return str - buf;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}
