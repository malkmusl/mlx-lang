/* <emmintrin.h>: the SSE2 intrinsics programs use for a few scalar
   conversions, written as plain functions over a 128-bit struct (the
   compiler has no vector registers): _mm_set_sd and _mm_cvttsd_si32/64
   (a double truncated to an integer, the "integer indefinite" value when
   it does not fit), the float forms, the integer loads and stores,
   _mm_pause; the packed double arithmetic WebCore's TransformationMatrix
   multiplies with (_mm_load_pd, _mm_add_pd, _mm_mul_pd, _mm_store_pd...).
   <xmmintrin.h> includes it. */
#ifndef _MLXCC_EMMINTRIN_H
#define _MLXCC_EMMINTRIN_H
#include <stdint.h>
#include <string.h>

typedef struct { double __d[2]; } __m128d;
typedef struct { float __f[4]; } __m128;
typedef struct { long long __q[2]; } __m128i;

static inline __m128d _mm_set_sd(double w) { __m128d r; r.__d[0] = w; r.__d[1] = 0.0; return r; }
static inline __m128d _mm_set1_pd(double w) { __m128d r; r.__d[0] = w; r.__d[1] = w; return r; }
static inline __m128d _mm_setzero_pd(void) { __m128d r; r.__d[0] = 0.0; r.__d[1] = 0.0; return r; }
static inline double _mm_cvtsd_f64(__m128d a) { return a.__d[0]; }
static inline __m128d _mm_set_pd(double hi, double lo) { __m128d r; r.__d[0] = lo; r.__d[1] = hi; return r; }
static inline __m128d _mm_setr_pd(double lo, double hi) { __m128d r; r.__d[0] = lo; r.__d[1] = hi; return r; }
static inline __m128d _mm_load_pd(const double *p) { __m128d r; r.__d[0] = p[0]; r.__d[1] = p[1]; return r; }
static inline __m128d _mm_loadu_pd(const double *p) { __m128d r; memcpy(&r, p, 16); return r; }
static inline __m128d _mm_load1_pd(const double *p) { return _mm_set1_pd(*p); }
#define _mm_load_pd1 _mm_load1_pd
static inline void _mm_store_pd(double *p, __m128d a) { p[0] = a.__d[0]; p[1] = a.__d[1]; }
static inline void _mm_storeu_pd(double *p, __m128d a) { memcpy(p, &a, 16); }
static inline __m128d _mm_add_pd(__m128d a, __m128d b) { a.__d[0] += b.__d[0]; a.__d[1] += b.__d[1]; return a; }
static inline __m128d _mm_sub_pd(__m128d a, __m128d b) { a.__d[0] -= b.__d[0]; a.__d[1] -= b.__d[1]; return a; }
static inline __m128d _mm_mul_pd(__m128d a, __m128d b) { a.__d[0] *= b.__d[0]; a.__d[1] *= b.__d[1]; return a; }
static inline __m128d _mm_div_pd(__m128d a, __m128d b) { a.__d[0] /= b.__d[0]; a.__d[1] /= b.__d[1]; return a; }
static inline __m128 _mm_set_ss(float w) { __m128 r; r.__f[0] = w; r.__f[1] = 0.0f; r.__f[2] = 0.0f; r.__f[3] = 0.0f; return r; }
static inline __m128 _mm_setzero_ps(void) { __m128 r; r.__f[0] = 0.0f; r.__f[1] = 0.0f; r.__f[2] = 0.0f; r.__f[3] = 0.0f; return r; }
static inline float _mm_cvtss_f32(__m128 a) { return a.__f[0]; }

static inline int _mm_cvttsd_si32(__m128d a) {
    double d = a.__d[0];
    if (!(d > -2147483649.0 && d < 2147483648.0)) return (int)0x80000000;
    return (int)d;
}
static inline long long _mm_cvttsd_si64(__m128d a) {
    double d = a.__d[0];
    if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0)) return (long long)0x8000000000000000LL;
    return (long long)d;
}
static inline int _mm_cvttss_si32(__m128 a) {
    float f = a.__f[0];
    if (!(f > -2147483649.0f && f < 2147483648.0f)) return (int)0x80000000;
    return (int)f;
}
static inline long long _mm_cvttss_si64(__m128 a) {
    float f = a.__f[0];
    if (!(f >= -9223372036854775808.0f && f < 9223372036854775808.0f)) return (long long)0x8000000000000000LL;
    return (long long)f;
}
#define _mm_cvttsd_si64x _mm_cvttsd_si64
#define _mm_cvttss_si64x _mm_cvttss_si64

static inline __m128i _mm_setzero_si128(void) { __m128i r; r.__q[0] = 0; r.__q[1] = 0; return r; }
static inline __m128i _mm_set_epi64x(long long hi, long long lo) { __m128i r; r.__q[0] = lo; r.__q[1] = hi; return r; }
static inline __m128i _mm_cvtsi64_si128(long long a) { return _mm_set_epi64x(0, a); }
static inline long long _mm_cvtsi128_si64(__m128i a) { return a.__q[0]; }
static inline int _mm_cvtsi128_si32(__m128i a) { return (int)a.__q[0]; }
static inline __m128i _mm_loadu_si128(const __m128i *p) { __m128i r; memcpy(&r, p, 16); return r; }
static inline __m128i _mm_load_si128(const __m128i *p) { __m128i r; memcpy(&r, p, 16); return r; }
static inline __m128i _mm_loadu_si64(const void *p) { __m128i r; r.__q[1] = 0; memcpy(&r.__q[0], p, 8); return r; }
static inline void _mm_storeu_si128(__m128i *p, __m128i a) { memcpy(p, &a, 16); }
static inline void _mm_store_si128(__m128i *p, __m128i a) { memcpy(p, &a, 16); }
static inline int _mm_movemask_epi8(__m128i a) {
    unsigned char b[16]; int mask = 0, i;
    memcpy(b, &a, 16);
    for (i = 0; i < 16; i++) if (b[i] & 0x80) mask |= 1 << i;
    return mask;
}
static inline __m128i _mm_packus_epi16(__m128i a, __m128i b) {
    short s[16]; unsigned char out[16]; int i; __m128i r;
    memcpy(s, &a, 16); memcpy(s + 8, &b, 16);
    for (i = 0; i < 16; i++) out[i] = s[i] < 0 ? 0 : s[i] > 255 ? 255 : (unsigned char)s[i];
    memcpy(&r, out, 16);
    return r;
}
static inline __m128i _mm_cmpeq_epi8(__m128i a, __m128i b) {
    unsigned char x[16], y[16]; int i; __m128i r;
    memcpy(x, &a, 16); memcpy(y, &b, 16);
    for (i = 0; i < 16; i++) x[i] = x[i] == y[i] ? 0xff : 0;
    memcpy(&r, x, 16);
    return r;
}
static inline __m128i _mm_or_si128(__m128i a, __m128i b) { a.__q[0] |= b.__q[0]; a.__q[1] |= b.__q[1]; return a; }
static inline __m128i _mm_and_si128(__m128i a, __m128i b) { a.__q[0] &= b.__q[0]; a.__q[1] &= b.__q[1]; return a; }
static inline __m128i _mm_xor_si128(__m128i a, __m128i b) { a.__q[0] ^= b.__q[0]; a.__q[1] ^= b.__q[1]; return a; }
static inline int _mm_test_all_zeros(__m128i mask, __m128i a) { return ((mask.__q[0] & a.__q[0]) | (mask.__q[1] & a.__q[1])) == 0; }
static inline void _mm_pause(void) { __asm__ volatile("pause"); }
static inline void _mm_mfence(void) { __asm__ volatile("mfence"); }
static inline void _mm_lfence(void) { __asm__ volatile("lfence"); }
static inline void _mm_sfence(void) { __asm__ volatile("sfence"); }
#endif
