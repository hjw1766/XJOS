#ifndef XJOS_ASSERT_H
#define XJOS_ASSERT_H


void assertion_failed(char *exp, char *file, char *base, int line);


// simple detect exp value
#define assert(exp) \
    if (exp)        \
        ;           \
    else            \
        assertion_failed(#exp, __FILE__, __BASE_FILE__, __LINE__)


void panic(const char *fmt, ...);



#endif /* XJOS_ASSERT_H */