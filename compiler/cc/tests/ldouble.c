/* long double as the x87 extended format (80 bits in 16): the size and
   alignment, float.h's limits, arithmetic in 64-bit mantissas, the
   conversions to and from the other types, comparisons, functions taking
   and returning long doubles (and through a variadic call), arrays and
   structs of them, a global initializer. Values beyond double's range
   are compared, not printed: mlxlibc's printf prints a long double
   exactly within that range only. gcc printed the expected output. */
#include <stdio.h>
#include <float.h>
#include <stdarg.h>
#include <stdint.h>

static long double scale = 1.5L;
static const long double third = 1.0L / 3.0L;
static long double table[3] = { 1e4000L, -2.5L, 0.1L };

struct pair { long double a; int b; };

static long double average(long double x, long double y) { return (x + y) / 2; }

static long double sum_of(int count, ...)
{
    va_list ap;
    long double total = 0;
    va_start(ap, count);
    for (int i = 0; i < count; i++) total += va_arg(ap, long double);
    va_end(ap);
    return total;
}

static struct pair make(long double a, int b) { struct pair p = { a, b }; return p; }

int main(void)
{
    long double x = 1.0L;
    long double y = x / 3;
    long double eps = LDBL_EPSILON;
    double dy = (double)y;

    printf("%zu %zu %d %d %d\n", sizeof(long double), _Alignof(long double), LDBL_MANT_DIG, LDBL_DIG, LDBL_MAX_EXP);
    printf("%.20Lg %.20Lg %.20Lg\n", y, third, 1 + eps);
    printf("%d %d %d\n", 1.0L + eps != 1.0L, 1.0L + eps / 2 == 1.0L, (double)(1.0L + eps) == 1.0);
    printf("%.17g %.21Lg %d\n", dy, y - dy, y > dy);
    printf("%d %d %d %d %d %d\n", LDBL_MAX > DBL_MAX, LDBL_MIN < DBL_MIN, LDBL_MIN > 0, LDBL_TRUE_MIN > 0, LDBL_TRUE_MIN < LDBL_MIN, LDBL_MAX / 1e4000L > 1e900L);
    printf("%.5Lg %.20Lg %.10Lg %d\n", table[0] / 1e3990L, table[1], table[2] * 10, table[0] > DBL_MAX);
    printf("%.3Lf %.3Lf %.3Lf\n", average(1, 2), scale * scale, sum_of(3, 1.25L, 2.5L, -0.75L));
    printf("%.1Lf %Lu %lld %u\n", (long double)18446744073709551615ULL, (unsigned long long)(long double)18446744073709551615ULL, (long long)-123456789.75L, (unsigned)1e9L);
    printf("%.17Lg %.17Lg %d\n", (long double)0.1, (long double)0.1f, (float)(1.0L / 3) == (float)(1.0 / 3));
    printf("%d %d %d %d %d %d\n", x < y, y < x, x == 1, y <= third, y != third, !y);
    long double z = -y;
    z += 1;
    z *= 3;
    z -= 2;
    z /= 4;
    printf("%.20Lg %d\n", z, z == 0);
    struct pair p = make(2.5L, 7);
    long double copy = p.a;
    p.a = p.a * copy;
    printf("%.3Lf %d %zu\n", p.a, p.b, sizeof p);
    long double q = 1;
    for (int i = 0; i < 60; i++) q *= 2;
    printf("%.0Lf %.0f %d\n", q + 1, (double)(q + 1), (int)((q + 1) - q));
    long double w = 1e308;
    w *= 1e10;
    printf("%.4Le %d %d\n", w / 1e300, w > DBL_MAX, (double)w == (double)1e308 * 1e10);
    long double neg = -0.0L;
    printf("%Lg %Lg %d\n", neg, -neg, neg == 0);
    printf("%.10Lf\n", 0.1L + 0.2L);
    return 0;
}
