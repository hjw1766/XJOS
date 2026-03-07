#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/math.h>


int cmd_float(int argc, char **argv, char **envp) {
    double x = 8.0;

    printf("sin(%f) = %f\n", x, sin(x));
    printf("cos(%f) = %f\n", x, cos(x));
    printf("tan(%f) = %f\n", x, tan(x));
    printf("sqrt(%f) = %f\n", x, sqrt(x));
    printf("log2(%f) = %f\n", x, log2(x));
    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char const *argv[], char const *envp[]) {
    return cmd_float(argc, (char **)argv, (char **)envp);
}
#endif