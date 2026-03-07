/* vsprintf.c -- Lars Wirzenius & Linus Torvalds, optimized with FPU support */

#include <xjos/stdarg.h>
#include <xjos/string.h>
#include <xjos/assert.h>

#define ZEROPAD 0x01   // 填充零
#define SIGN    0x02   // 有符号/无符号
#define PLUS    0x04   // 显示正号
#define SPACE   0x08   // 若为正，置空格
#define LEFT    0x10   // 左对齐
#define SPECIAL 0x20   // 0x 或 0
#define SMALL   0x40   // 小写字母
#define DOUBLE  0x80   // 浮点数标志

#define is_digit(c) ((c) >= '0' && (c) <= '9')

// 汇编级优化的 skip_atoi
static int skip_atoi(const char **s) {
    int i = 0;
    while (is_digit(**s)) {
        i = (i << 3) + (i << 1) + (*((*s)++) - '0'); 
    }
    return i;
}

// 核心转换函数：支持 double 指针处理与内联汇编加速
static char *number(char *str, u32 *num, int base, int size, int precision, int flags) {
    char pad, sign, tmp[64]; 
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i = 0;

    if (flags & SMALL) digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (flags & LEFT) flags &= ~ZEROPAD;
    if (base < 2 || base > 36) return 0;

    pad = (flags & ZEROPAD) ? '0' : ' ';

    // 符号逻辑处理
    if (flags & DOUBLE) {
        if (*(double *)num < 0) {
            sign = '-';
            *(double *)num = -(*(double *)num);
        } else {
            sign = (flags & PLUS) ? '+' : ((flags & SPACE) ? ' ' : 0);
        }
    } else if (flags & SIGN && (int)(*num) < 0) {
        sign = '-';
        *num = -(int)(*num);
    } else {
        sign = (flags & PLUS) ? '+' : ((flags & SPACE) ? ' ' : 0);
    }

    if (sign) size--;

    if (flags & SPECIAL) {
        if (base == 16) size -= 2;
        else if (base == 8) size--;
    }

    // 数值转字符串
    if (flags & DOUBLE) {
        double d = *(double *)num;
        u32 ival = (u32)d;
        u32 fval = (u32)((d - ival) * 1000000); // 默认 6 位精度
        
        // 处理小数
        do {
            tmp[i++] = digits[fval % base];
            fval /= base;
        } while (fval);
        
        while (i < 6) tmp[i++] = '0';
        tmp[i++] = '.';

        // 处理整数（内联汇编）
        char *p = &tmp[i];
        __asm__ __volatile__ (
            "1: xorl %%edx, %%edx \n\t"
            "divl %[base] \n\t"
            "movb (%[digits],%%edx,1), %%bl \n\t"
            "movb %%bl,(%[p]) \n\t"
            "leal 1(%[p]),%[p] \n\t"
            "testl %%eax, %%eax \n\t"
            "jne 1b"
            : [p] "+r"(p), "+a"(ival)
            : [base] "r"(base), [digits] "r"(digits)
            : "edx", "ebx", "memory", "cc"
        );
        i = p - tmp;
    } else if (*num == 0) {
        tmp[i++] = '0';
    } else {
        u32 val = *num;
        char *p = tmp;
        __asm__ __volatile__ (
            "1: xorl %%edx, %%edx \n\t"
            "divl %[base] \n\t"
            "movb (%[digits],%%edx,1), %%bl \n\t"
            "movb %%bl,(%[p]) \n\t"
            "leal 1(%[p]),%[p] \n\t"
            "testl %%eax, %%eax \n\t"
            "jne 1b"
            : [p] "+r"(p), "+a"(val)
            : [base] "r"(base), [digits] "r"(digits)
            : "edx", "ebx", "memory", "cc"
        );
        i = p - tmp;
    }

    if (i > precision) precision = i;
    size -= precision;

    if (!(flags & (ZEROPAD + LEFT)))
        while (size-- > 0) *str++ = ' ';

    if (sign) *str++ = sign;

    if (flags & SPECIAL) {
        if (base == 8) *str++ = '0';
        else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33]; 
        }
    }

    if (!(flags & LEFT))
        while (size-- > 0) *str++ = pad;

    while (i < precision--) *str++ = '0';
    while (i-- > 0) *str++ = tmp[i];
    while (size-- > 0) *str++ = ' ';

    return str;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
    int len, i, flags, field_width, precision, qualifier;
    char *str, *s;
    int *ip;
    u32 num;

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
        if (is_digit(*fmt)) field_width = skip_atoi(&fmt);
        else if (*fmt == '*') {
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
            if (is_digit(*fmt)) precision = skip_atoi(&fmt);
            else if (*fmt == '*') {
                ++fmt;
                precision = va_arg(args, int);
            }
            if (precision < 0) precision = 0;
        }

        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }

        switch (*fmt) {
            case 'c':
                if (!(flags & LEFT)) while (--field_width > 0) *str++ = ' ';
                *str++ = (unsigned char)va_arg(args, int);
                while (--field_width > 0) *str++ = ' ';
                break;

            case 's':
                s = va_arg(args, char *);
                if (!s) s = "<NULL>";
                len = strlen(s);
                if (precision >= 0 && len > precision) len = precision;
                if (!(flags & LEFT)) while (len < field_width--) *str++ = ' ';
                for (i = 0; i < len; ++i) *str++ = *s++;
                while (len < field_width--) *str++ = ' ';
                break;

            case 'o':
                num = va_arg(args, unsigned long);
                str = number(str, &num, 8, field_width, precision, flags);
                break;

            case 'p':
                if (field_width == -1) {
                    field_width = 8;
                    flags |= ZEROPAD;
                }
                num = (unsigned long)va_arg(args, void *);
                str = number(str, &num, 16, field_width, precision, flags);
                break;

            case 'x': flags |= SMALL;
            case 'X':
                num = va_arg(args, unsigned long);
                str = number(str, &num, 16, field_width, precision, flags);
                break;

            case 'd':
            case 'i': flags |= SIGN;
            case 'u':
                num = va_arg(args, unsigned long);
                str = number(str, &num, 10, field_width, precision, flags);
                break;

            case 'f':
                flags |= SIGN | DOUBLE;
                double dnum = va_arg(args, double);
                str = number(str, (u32 *)&dnum, 10, field_width, precision, flags);
                break;

            case 'n':
                ip = va_arg(args, int *);
                *ip = (str - buf);
                break;

            default:
                if (*fmt != '%') *str++ = '%';
                if (*fmt) *str++ = *fmt;
                else --fmt;
                break;
        }
    }
    *str = '\0';
    return str - buf;
}

// 标准的可变参数入口
int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

    return i;
}