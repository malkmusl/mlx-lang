/* The long double functions of <math.h>: glibc's on glibc, mlxlibc's x87
   code (tools/libc/x87_long.s) on mlxlibc, the same answers either way.
   The exact ones (roots, rounding, remainders, scaling, signs) are
   printed to 18 digits, the transcendental ones to 15, within an ulp of
   each other's 64-bit results. gcc printed the expected output. */
#include <stdio.h>
#include <math.h>

static void line(const char *name, long double v) { printf("%s %.15Lg\n", name, v); }
static void exact(const char *name, long double v) { printf("%s %.18Lg\n", name, v); }

int main(void)
{
    long double x = 0.7L, y = 2.5L, big = 123456789.125L;
    int e;
    long double whole;

    exact("sqrt", sqrtl(2.0L));
    exact("fabs", fabsl(-3.25L));
    exact("floor", floorl(-2.5L));
    exact("ceil", ceill(-2.5L));
    exact("trunc", truncl(-2.5L));
    exact("round", roundl(2.5L));
    exact("round-", roundl(-2.5L));
    exact("round+", roundl(0.49999999999999999997L));
    exact("rint", rintl(2.5L));
    exact("rint3", rintl(3.5L));
    exact("nearby", nearbyintl(-0.5L));
    printf("lrint %ld %ld %lld\n", lrintl(2.5L), lroundl(2.5L), llroundl(-7.5L));
    exact("fmod", fmodl(big, 0.3L));
    exact("fmod-", fmodl(-5.5L, 2.0L));
    exact("remainder", remainderl(5.5L, 2.0L));
    exact("ldexp", ldexpl(1.5L, 100));
    exact("ldexp-", ldexpl(3.0L, -1000));
    exact("scalbn", scalbnl(1.0L, 1000));
    exact("frexp", frexpl(big, &e));
    printf("frexp-e %d\n", e);
    exact("frexp0", frexpl(0.0L, &e));
    printf("frexp0-e %d\n", e);
    exact("modf", modfl(-3.75L, &whole));
    exact("modf-whole", whole);
    exact("copysign", copysignl(3.0L, -0.0L));
    exact("fmin", fminl(x, y));
    exact("fmax", fmaxl(x, y));
    exact("fdim", fdiml(y, x));
    exact("fdim0", fdiml(x, y));
    exact("hypot", hypotl(3.0L, 4.0L));
    exact("cbrt8", cbrtl(-8.0L));
    exact("pow2", powl(2.0L, 10.0L));
    exact("pow-", powl(-2.0L, 3.0L));
    exact("pow0", powl(0.0L, 5.0L));
    exact("pow00", powl(7.0L, 0.0L));
    printf("pow-inf %d %d %d\n", isinf(powl(0.0L, -1.0L)) != 0, isnan(powl(-8.0L, 1.0L / 3)) != 0, isinf(expl(5000.0L)) != 0);
    printf("specials %d %d %d %d\n", expl(-INFINITY) == 0, isinf(expl(INFINITY)) != 0, isinf(logl(0.0L)) != 0 && logl(0.0L) < 0, isnan(sqrtl(-1.0L)) != 0);

    line("sin", sinl(x));
    line("cos", cosl(x));
    line("tan", tanl(x));
    line("asin", asinl(x));
    line("acos", acosl(x));
    line("acos-", acosl(-x));
    line("atan", atanl(y));
    line("atan2", atan2l(-1.0L, -1.0L));
    line("exp", expl(x));
    line("exp2", exp2l(y));
    line("exp-", expl(-20.5L));
    line("log", logl(y));
    line("log2", log2l(10.0L));
    line("log10", log10l(y));
    line("pow", powl(y, x));
    line("pow-frac", powl(2.0L, 0.5L));
    line("cbrt", cbrtl(10.0L));
    line("sinh", sinhl(y));
    line("cosh", coshl(y));
    line("tanh", tanhl(x));
    line("hypot2", hypotl(x, y));
    line("big-sin", sinl(1000.0L));
    return 0;
}
