#ifndef XJOS_DEBUG_H
#define XJOS_DEBUG_H


#ifndef MM_TRACE
#define MM_TRACE 0
#endif

#if MM_TRACE
#define MM_TRACEK(fmt, args...) DEBUGK(fmt, ##args)
#else
#define MM_TRACEK(fmt, args...) ((void)0)
#endif


void debug(char *file, int line, const char *fmt, ...);


#define DEBUGK(fmt, args...) debug(__BASE_FILE__, __LINE__, fmt, ##args);
#define BMB asm volatile("xchgw %bx, %bx\n")





#endif /* XJOS_DEBUG_H */