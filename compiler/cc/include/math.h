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
int __fpclassify(double);
int __fpclassifyf(float);
int __signbit(double);
int __signbitf(float);
#ifndef __cplusplus
#define fpclassify(x) (sizeof(x) == sizeof(float) ? __fpclassifyf(x) : __fpclassify(x))
#define isnormal(x) (fpclassify(x) == FP_NORMAL)
#define isnan(x) ((x) != (x))
#define isinf(x) (!isnan(x) && isnan((x) - (x)))
#define isfinite(x) (!isnan((x) - (x)))
#define signbit(x) (sizeof(x) == sizeof(float) ? __signbitf(x) : __signbit(x))
#else
/* C++: the classification macros are functions (std::isnan(x) has to
   stay a call), one per floating type. */
extern "C++" {
inline int fpclassify(float x) { return __fpclassifyf(x); }
inline int fpclassify(double x) { return __fpclassify(x); }
inline int fpclassify(long double x) { return x != x ? FP_NAN : __fpclassify((double)x); }
inline bool isnan(float x) { return x != x; }
inline bool isnan(double x) { return x != x; }
inline bool isnan(long double x) { return x != x; }
inline bool isinf(float x) { return x == x && (x - x) != (x - x); }
inline bool isinf(double x) { return x == x && (x - x) != (x - x); }
inline bool isinf(long double x) { return x == x && (x - x) != (x - x); }
inline bool isfinite(float x) { return (x - x) == (x - x); }
inline bool isfinite(double x) { return (x - x) == (x - x); }
inline bool isfinite(long double x) { return (x - x) == (x - x); }
inline bool isnormal(float x) { return __fpclassifyf(x) == FP_NORMAL; }
inline bool isnormal(double x) { return __fpclassify(x) == FP_NORMAL; }
inline bool isnormal(long double x) { return fpclassify(x) == FP_NORMAL; }
inline bool signbit(float x) { return __signbitf(x) != 0; }
inline bool signbit(double x) { return __signbit(x) != 0; }
inline bool signbit(long double x) { return __signbit((double)x) != 0; }
inline bool isunordered(double a, double b) { return a != a || b != b; }
inline bool isgreater(double a, double b) { return !isunordered(a, b) && a > b; }
inline bool isgreaterequal(double a, double b) { return !isunordered(a, b) && a >= b; }
inline bool isless(double a, double b) { return !isunordered(a, b) && a < b; }
inline bool islessequal(double a, double b) { return !isunordered(a, b) && a <= b; }
inline bool islessgreater(double a, double b) { return !isunordered(a, b) && a != b; }
}
#endif
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
static inline int ilogbf(float x) { return ilogb((double)x); }
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
/* The long double functions: glibc's libm, and on mlxlibc x87 code of its own. */
long double sinl(long double);
long double cosl(long double);
long double tanl(long double);
long double asinl(long double);
long double acosl(long double);
long double atanl(long double);
long double atan2l(long double, long double);
long double sinhl(long double);
long double coshl(long double);
long double tanhl(long double);
long double expl(long double);
long double exp2l(long double);
long double logl(long double);
long double log10l(long double);
long double log2l(long double);
long double powl(long double, long double);
long double sqrtl(long double);
long double cbrtl(long double);
long double hypotl(long double, long double);
long double fabsl(long double);
long double floorl(long double);
long double ceill(long double);
long double truncl(long double);
long double roundl(long double);
long double rintl(long double);
long double nearbyintl(long double);
long lroundl(long double);
long long llroundl(long double);
long lrintl(long double);
long long llrintl(long double);
long double fmodl(long double, long double);
long double remainderl(long double, long double);
long double ldexpl(long double, int);
long double scalbnl(long double, int);
long double scalblnl(long double, long);
long double frexpl(long double, int *);
long double modfl(long double, long double *);
long double fminl(long double, long double);
long double fmaxl(long double, long double);
long double fdiml(long double, long double);
long double copysignl(long double, long double);
#endif
