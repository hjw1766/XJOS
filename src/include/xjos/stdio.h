#ifndef XJOS_STDIO_H
#define XJOS_STDIO_H

#include <xjos/stdarg.h>


int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt,...);
int printf(const char *fmt,...);



#endif /* XJOS_STDIO_H */