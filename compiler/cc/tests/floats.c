/* Floating point: arithmetic, conversions, calls, printf. */
#include <stdio.h>

static double average(int count, double a, double b, double c) { return (a + b + c) / count; }
static float half(float x) { return x / 2; }
static double many(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10;
}
static long mixed(int a, double b, long c, float d, char e, double f, int g, int h, int i, int k, double l) {
    return a + (long)b + c + (long)d + e + (long)f + g + h + i + k + (long)l;
}

double global = 2.5;
float gf = -0.75f;
double table[] = { 1.0, 1e10, 1e-3, -0.0 };

int main(void) {
    double x = 1.5, y = -2.25;
    printf("%.4f %.4f %.4f %.4f\n", x + y, x - y, x * y, x / y);
    printf("%d %d %d %d\n", x < y, x > y, x == 1.5, x != 1.5);
    float f = 3.75f;
    printf("%f %g\n", f, half(f));
    printf("%g\n", average(3, 1.0, 2.0, 4.5));
    printf("%g\n", many(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    printf("%ld\n", mixed(1, 2.9, 3, 4.5f, 5, 6.1, 7, 8, 9, 10, 11.99));
    int i = (int)-7.9;
    unsigned u = (unsigned)4000000000.0;
    long l = (long)1e18;
    unsigned long ul = (unsigned long)1.8e19;
    printf("%d %u %ld %lu\n", i, u, l, ul);
    double fromU = (double)18446744073709551615ul;
    double fromI = -5;
    printf("%.1f %.1f\n", fromU, fromI);
    printf("%g %g %g %g\n", global, gf, table[1], table[2]);
    double z = 0.0;
    double nan = z / z;
    printf("%d %d %d\n", nan == nan, nan != nan, nan < 1.0);
    printf("%d\n", !z);
    double acc = 0;
    for (int k = 1; k <= 10; k++) acc += 1.0 / k;
    printf("%.10f\n", acc);
    float fs = 0.1f;
    double ds = fs;
    printf("%.10f\n", ds);
    printf("%e %E\n", 123456.789, 0.000123);
    double neg = -x;
    printf("%g %g\n", neg, -f);
    x += 1; x *= 2; x -= 0.5; x /= 4;
    printf("%g\n", x);
    printf("%d\n", (int)(0.1 + 0.2 == 0.3));
    _Bool b = 0.0001;
    printf("%d\n", b);
    char c = 65.7;
    printf("%c\n", c);
    printf("%g\n", 1e308 * 10);
    return 0;
}
