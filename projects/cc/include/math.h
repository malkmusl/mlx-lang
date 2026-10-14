#ifndef _MLXCC_MATH_H
#define _MLXCC_MATH_H
/* The functions are libm's: link with -lm. */
#define HUGE_VAL (1e308 * 10.0)
#define HUGE_VALF ((float)HUGE_VAL)
#define INFINITY ((float)HUGE_VAL)
#define NAN (0.0f / 0.0f)
#define M_E 2.7182818284590452354
#define M_LOG2E 1.4426950408889634074
#define M_LOG10E 0.43429448190325182765
#define M_LN2 0.69314718055994530942
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_1_PI 0.31830988618379067154
#define M_2_PI 0.63661977236758134308
#define M_SQRT2 1.41421356237309504880
#define M_SQRT1_2 0.70710678118654752440
#define FP_NAN 0
#define FP_INFINITE 1
#define FP_ZERO 2
#define FP_SUBNORMAL 3
#define FP_NORMAL 4
#define fpclassify(x) (sizeof(x) == sizeof(float) ? __fpclassifyf(x) : __fpclassify(x))
int __fpclassify(double);
int __fpclassifyf(float);
#define isnormal(x) (fpclassify(x) == FP_NORMAL)
#define isnan(x) ((x) != (x))
#define isinf(x) (!isnan(x) && isnan((x) - (x)))
#define isfinite(x) (!isnan((x) - (x)))
#define signbit(x) (sizeof(x) == sizeof(float) ? __signbitf(x) : __signbit(x))
int __signbit(double);
int __signbitf(float);
double sin(double);
double cos(double);
double tan(double);
double asin(double);
double acos(double);
double atan(double);
double atan2(double, double);
double sinh(double);
double cosh(double);
double tanh(double);
double exp(double);
double exp2(double);
double expm1(double);
double log(double);
double log2(double);
double log10(double);
double log1p(double);
double pow(double, double);
double sqrt(double);
double cbrt(double);
double hypot(double, double);
double fabs(double);
double floor(double);
double ceil(double);
double round(double);
double trunc(double);
double rint(double);
double nearbyint(double);
long lround(double);
long lrint(double);
double fmod(double, double);
double remainder(double, double);
double modf(double, double *);
double frexp(double, int *);
double ldexp(double, int);
double scalbn(double, int);
double copysign(double, double);
double nextafter(double, double);
double fmin(double, double);
double fmax(double, double);
double fdim(double, double);
double fma(double, double, double);
float sinf(float);
float cosf(float);
float tanf(float);
float sqrtf(float);
float fabsf(float);
float floorf(float);
float ceilf(float);
float roundf(float);
float powf(float, float);
float expf(float);
float logf(float);
float atan2f(float, float);
float fmodf(float, float);
double acosh(double);
double asinh(double);
double atanh(double);
double erf(double);
double erfc(double);
double lgamma(double);
double tgamma(double);
double logb(double);
int ilogb(double);
long long llrint(double);
long long llround(double);
double scalbln(double, long);
double remquo(double, double, int *);
double nan(const char *);
double j0(double);
double j1(double);
double y0(double);
double y1(double);
float asinf(float);
float acosf(float);
float atanf(float);
float sinhf(float);
float coshf(float);
float tanhf(float);
float asinhf(float);
float acoshf(float);
float atanhf(float);
float exp2f(float);
float expm1f(float);
float log2f(float);
float log10f(float);
float log1pf(float);
float cbrtf(float);
float hypotf(float, float);
float truncf(float);
float rintf(float);
float nearbyintf(float);
long lroundf(float);
long lrintf(float);
long long llroundf(float);
long long llrintf(float);
float remainderf(float, float);
float modff(float, float *);
float frexpf(float, int *);
float ldexpf(float, int);
float scalbnf(float, int);
float copysignf(float, float);
float nextafterf(float, float);
float fminf(float, float);
float fmaxf(float, float);
float fdimf(float, float);
float fmaf(float, float, float);
float erff(float);
float erfcf(float);
float lgammaf(float);
float tgammaf(float);
float nanf(const char *);
/* long double is double here (projects/cc/README.md): these are the
   double functions under their names. */
#define sinl(x) sin(x)
#define cosl(x) cos(x)
#define tanl(x) tan(x)
#define expl(x) exp(x)
#define logl(x) log(x)
#define powl(x, y) pow(x, y)
#define sqrtl(x) sqrt(x)
#define fabsl(x) fabs(x)
#define floorl(x) floor(x)
#define ceill(x) ceil(x)
#define fmodl(x, y) fmod(x, y)
#define ldexpl(x, e) ldexp(x, e)
#define frexpl(x, e) frexp(x, e)
#define modfl(x, p) modf(x, (double *)(p))
#endif
