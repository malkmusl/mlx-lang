/* Variadic functions of the program's own, and va_list passed on. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int sum(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int total = 0;
    for (int i = 0; i < count; i++) total += va_arg(ap, int);
    va_end(ap);
    return total;
}

static double dsum(int count, ...) {
    va_list ap;
    va_start(ap, count);
    double total = 0;
    for (int i = 0; i < count; i++) total += va_arg(ap, double);
    va_end(ap);
    return total;
}

static void say(const char *format, ...) {
    char buffer[128];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, sizeof buffer, format, ap);
    va_end(ap);
    printf("[%s]\n", buffer);
}

static long mixture(const char *kinds, ...) {
    va_list ap, copy;
    va_start(ap, kinds);
    va_copy(copy, ap);
    long total = 0;
    for (const char *k = kinds; *k; k++) {
        if (*k == 'i') total += va_arg(ap, int);
        else if (*k == 'l') total += va_arg(ap, long);
        else if (*k == 'd') total += (long)va_arg(ap, double);
        else if (*k == 's') total += (long)strlen(va_arg(ap, char *));
    }
    long first = va_arg(copy, int);
    va_end(copy);
    va_end(ap);
    return total * 1000 + first;
}

int main(void) {
    printf("%d\n", sum(3, 1, 2, 3));
    printf("%d\n", sum(10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    printf("%g\n", dsum(4, 0.5, 1.5, 2.5, 3.5));
    printf("%g\n", dsum(10, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0));
    say("%d-%s-%.2f-%c", 42, "text", 3.14159, 'x');
    say("%5d|%-5d|%05d", 7, 7, 7);
    printf("%ld\n", mixture("idlsidl", 7, 2.5, 100L, "four", 1, 3.0, 2L));
    printf("%s %d %c %5.1f %x %o %%\n", "all", -12, 'Z', 2.25, 255, 8);
    char buffer[64];
    int n = sprintf(buffer, "%d+%d=%d", 2, 2, 4);
    printf("%s %d\n", buffer, n);
    return 0;
}
