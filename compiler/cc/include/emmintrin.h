/* <emmintrin.h>: the SSE2 intrinsics, all of them (x86-64's baseline:
   SIMDe's native x86 path, which WebKit's SIMD helpers take on x86-64,
   uses them; mlx-cc does not predefine __SSE2__, having no GCC vector
   types: <mmintrin.h>), over __m128d and __m128i that are unions of their
   lanes (the compiler has no vector registers): each a plain function
   with the instruction's result, wrapping or saturating, comparisons as
   all-ones lanes, shifts past the lane's width clearing it (or filling
   it with the sign), the conversions rounding to nearest even and giving
   the "integer indefinite" value when out of range. <xmmintrin.h> and
   <mmintrin.h> come with it. */
#ifndef _MLXCC_EMMINTRIN_H
#define _MLXCC_EMMINTRIN_H
#include <stdint.h>
#include <string.h>
#include <xmmintrin.h>

typedef union __attribute__((__aligned__(16))) {
    double __d[2];
    long long __q[2];
    unsigned long long __uq[2];
} __m128d;
typedef union __attribute__((__aligned__(16))) {
    long long __q[2];
    unsigned long long __uq[2];
    int __i[4];
    unsigned int __ui[4];
    short __s[8];
    unsigned short __us[8];
    signed char __b[16];
    unsigned char __ub[16];
} __m128i;
typedef __m128d __m128d_u;
typedef __m128i __m128i_u;

#define _MM_SHUFFLE2(x, y) (((x) << 1) | (y))

static inline void _mm_lfence(void) { __asm__ volatile("lfence" ::: "memory"); }
static inline void _mm_mfence(void) { __asm__ volatile("mfence" ::: "memory"); }
static inline void _mm_clflush(const void *p) { __asm__ volatile("clflush %0" : : "m"(*(const char *)p) : "memory"); }

/* Casts: the same 128 bits. */
static inline __m128 _mm_castpd_ps(__m128d a) { __m128 r; memcpy(&r, &a, 16); return r; }
static inline __m128i _mm_castpd_si128(__m128d a) { __m128i r; memcpy(&r, &a, 16); return r; }
static inline __m128d _mm_castps_pd(__m128 a) { __m128d r; memcpy(&r, &a, 16); return r; }
static inline __m128i _mm_castps_si128(__m128 a) { __m128i r; memcpy(&r, &a, 16); return r; }
static inline __m128 _mm_castsi128_ps(__m128i a) { __m128 r; memcpy(&r, &a, 16); return r; }
static inline __m128d _mm_castsi128_pd(__m128i a) { __m128d r; memcpy(&r, &a, 16); return r; }

/* Doubles. */
static inline __m128d _mm_setzero_pd(void) { __m128d r; r.__q[0] = 0; r.__q[1] = 0; return r; }
static inline __m128d _mm_undefined_pd(void) { return _mm_setzero_pd(); }
static inline __m128d _mm_set_sd(double w) { __m128d r; r.__d[0] = w; r.__d[1] = 0.0; return r; }
static inline __m128d _mm_set_pd(double hi, double lo) { __m128d r; r.__d[0] = lo; r.__d[1] = hi; return r; }
static inline __m128d _mm_setr_pd(double lo, double hi) { return _mm_set_pd(hi, lo); }
static inline __m128d _mm_set1_pd(double w) { return _mm_set_pd(w, w); }
#define _mm_set_pd1 _mm_set1_pd
static inline double _mm_cvtsd_f64(__m128d a) { return a.__d[0]; }
static inline __m128d _mm_loadu_pd(const double *p) { __m128d r; memcpy(&r, p, 16); return r; }
static inline __m128d _mm_load_pd(const double *p) { return _mm_loadu_pd(p); }
static inline __m128d _mm_load_sd(const double *p) { return _mm_set_sd(*p); }
static inline __m128d _mm_load1_pd(const double *p) { return _mm_set1_pd(*p); }
#define _mm_load_pd1 _mm_load1_pd
static inline __m128d _mm_loadr_pd(const double *p) { return _mm_set_pd(p[0], p[1]); }
static inline __m128d _mm_loadh_pd(__m128d a, const double *p) { a.__d[1] = *p; return a; }
static inline __m128d _mm_loadl_pd(__m128d a, const double *p) { a.__d[0] = *p; return a; }
static inline void _mm_storeu_pd(double *p, __m128d a) { memcpy(p, &a, 16); }
static inline void _mm_store_pd(double *p, __m128d a) { _mm_storeu_pd(p, a); }
static inline void _mm_store_sd(double *p, __m128d a) { *p = a.__d[0]; }
static inline void _mm_store1_pd(double *p, __m128d a) { p[0] = p[1] = a.__d[0]; }
#define _mm_store_pd1 _mm_store1_pd
static inline void _mm_storer_pd(double *p, __m128d a) { p[0] = a.__d[1]; p[1] = a.__d[0]; }
static inline void _mm_storeh_pd(double *p, __m128d a) { *p = a.__d[1]; }
static inline void _mm_storel_pd(double *p, __m128d a) { *p = a.__d[0]; }
static inline void _mm_stream_pd(double *p, __m128d a) { _mm_storeu_pd(p, a); }
static inline __m128d _mm_move_sd(__m128d a, __m128d b) { a.__d[0] = b.__d[0]; return a; }

#define __MLXCC_MAPPD(name, F, expr) \
    static inline __m128d name##_pd(__m128d a, __m128d b) { __m128d r; int i; for (i = 0; i < 2; i++) { double x = a.__d[i], y = b.__d[i]; (void)x; (void)y; r.F[i] = (expr); } return r; } \
    static inline __m128d name##_sd(__m128d a, __m128d b) { double x = a.__d[0], y = b.__d[0]; (void)x; (void)y; a.F[0] = (expr); return a; }
__MLXCC_MAPPD(_mm_add, __d, x + y)
__MLXCC_MAPPD(_mm_sub, __d, x - y)
__MLXCC_MAPPD(_mm_mul, __d, x * y)
__MLXCC_MAPPD(_mm_div, __d, x / y)
__MLXCC_MAPPD(_mm_min, __d, x < y ? x : y)
__MLXCC_MAPPD(_mm_max, __d, x > y ? x : y)
__MLXCC_MAPPD(_mm_cmpeq, __q, x == y ? -1 : 0)
__MLXCC_MAPPD(_mm_cmplt, __q, x < y ? -1 : 0)
__MLXCC_MAPPD(_mm_cmple, __q, x <= y ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpgt, __q, x > y ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpge, __q, x >= y ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpneq, __q, !(x == y) ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpnlt, __q, !(x < y) ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpnle, __q, !(x <= y) ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpngt, __q, !(x > y) ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpnge, __q, !(x >= y) ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpord, __q, (x == x && y == y) ? -1 : 0)
__MLXCC_MAPPD(_mm_cmpunord, __q, (x != x || y != y) ? -1 : 0)
static inline __m128d _mm_sqrt_pd(__m128d a) { a.__d[0] = __mlxcc_sqrt(a.__d[0]); a.__d[1] = __mlxcc_sqrt(a.__d[1]); return a; }
static inline __m128d _mm_sqrt_sd(__m128d a, __m128d b) { a.__d[0] = __mlxcc_sqrt(b.__d[0]); return a; }
static inline __m128d _mm_and_pd(__m128d a, __m128d b) { a.__q[0] &= b.__q[0]; a.__q[1] &= b.__q[1]; return a; }
static inline __m128d _mm_andnot_pd(__m128d a, __m128d b) { b.__q[0] &= ~a.__q[0]; b.__q[1] &= ~a.__q[1]; return b; }
static inline __m128d _mm_or_pd(__m128d a, __m128d b) { a.__q[0] |= b.__q[0]; a.__q[1] |= b.__q[1]; return a; }
static inline __m128d _mm_xor_pd(__m128d a, __m128d b) { a.__q[0] ^= b.__q[0]; a.__q[1] ^= b.__q[1]; return a; }
#define __MLXCC_COMISD(op, expr) \
    static inline int _mm_comi##op##_sd(__m128d a, __m128d b) { double x = a.__d[0], y = b.__d[0]; return (expr); } \
    static inline int _mm_ucomi##op##_sd(__m128d a, __m128d b) { double x = a.__d[0], y = b.__d[0]; return (expr); }
__MLXCC_COMISD(eq, x == y)
__MLXCC_COMISD(lt, x < y)
__MLXCC_COMISD(le, x <= y)
__MLXCC_COMISD(gt, x > y)
__MLXCC_COMISD(ge, x >= y)
__MLXCC_COMISD(neq, !(x == y))
static inline __m128d _mm_shuffle_pd(__m128d a, __m128d b, int imm) { return _mm_set_pd(b.__d[(imm >> 1) & 1], a.__d[imm & 1]); }
static inline __m128d _mm_unpackhi_pd(__m128d a, __m128d b) { return _mm_set_pd(b.__d[1], a.__d[1]); }
static inline __m128d _mm_unpacklo_pd(__m128d a, __m128d b) { return _mm_set_pd(b.__d[0], a.__d[0]); }
static inline int _mm_movemask_pd(__m128d a) { return (a.__q[0] < 0 ? 1 : 0) | (a.__q[1] < 0 ? 2 : 0); }

/* Conversions. */
static inline int _mm_cvtsd_si32(__m128d a) { return __mlxcc_cvt32(__mlxcc_rint(a.__d[0])); }
static inline long long _mm_cvtsd_si64(__m128d a) { return __mlxcc_cvt64(__mlxcc_rint(a.__d[0])); }
#define _mm_cvtsd_si64x _mm_cvtsd_si64
static inline int _mm_cvttsd_si32(__m128d a) { return __mlxcc_cvt32(a.__d[0]); }
static inline long long _mm_cvttsd_si64(__m128d a) { return __mlxcc_cvt64(a.__d[0]); }
#define _mm_cvttsd_si64x _mm_cvttsd_si64
static inline __m128d _mm_cvtsi32_sd(__m128d a, int b) { a.__d[0] = b; return a; }
static inline __m128d _mm_cvtsi64_sd(__m128d a, long long b) { a.__d[0] = (double)b; return a; }
#define _mm_cvtsi64x_sd _mm_cvtsi64_sd
static inline __m128 _mm_cvtsd_ss(__m128 a, __m128d b) { a.__f[0] = (float)b.__d[0]; return a; }
static inline __m128d _mm_cvtss_sd(__m128d a, __m128 b) { a.__d[0] = b.__f[0]; return a; }
static inline __m128d _mm_cvtps_pd(__m128 a) { return _mm_set_pd(a.__f[1], a.__f[0]); }
static inline __m128 _mm_cvtpd_ps(__m128d a) { __m128 r = _mm_setzero_ps(); r.__f[0] = (float)a.__d[0]; r.__f[1] = (float)a.__d[1]; return r; }
static inline __m128d _mm_cvtpi32_pd(__m64 a) { return _mm_set_pd(a.__i[1], a.__i[0]); }
static inline __m64 _mm_cvtpd_pi32(__m128d a) { __m64 r; r.__i[0] = __mlxcc_cvt32(__mlxcc_rint(a.__d[0])); r.__i[1] = __mlxcc_cvt32(__mlxcc_rint(a.__d[1])); return r; }
static inline __m64 _mm_cvttpd_pi32(__m128d a) { __m64 r; r.__i[0] = __mlxcc_cvt32(a.__d[0]); r.__i[1] = __mlxcc_cvt32(a.__d[1]); return r; }

/* Integers. */
static inline __m128i _mm_setzero_si128(void) { __m128i r; r.__q[0] = 0; r.__q[1] = 0; return r; }
static inline __m128i _mm_undefined_si128(void) { return _mm_setzero_si128(); }
static inline __m128i _mm_set_epi64x(long long hi, long long lo) { __m128i r; r.__q[0] = lo; r.__q[1] = hi; return r; }
static inline __m128i _mm_set_epi64(__m64 hi, __m64 lo) { return _mm_set_epi64x(hi.__q, lo.__q); }
static inline __m128i _mm_set_epi32(int e3, int e2, int e1, int e0) { __m128i r; r.__i[0] = e0; r.__i[1] = e1; r.__i[2] = e2; r.__i[3] = e3; return r; }
static inline __m128i _mm_set_epi16(short e7, short e6, short e5, short e4, short e3, short e2, short e1, short e0) {
    __m128i r; r.__s[0] = e0; r.__s[1] = e1; r.__s[2] = e2; r.__s[3] = e3; r.__s[4] = e4; r.__s[5] = e5; r.__s[6] = e6; r.__s[7] = e7; return r;
}
static inline __m128i _mm_set_epi8(char e15, char e14, char e13, char e12, char e11, char e10, char e9, char e8,
                                   char e7, char e6, char e5, char e4, char e3, char e2, char e1, char e0) {
    __m128i r;
    r.__b[0] = e0; r.__b[1] = e1; r.__b[2] = e2; r.__b[3] = e3; r.__b[4] = e4; r.__b[5] = e5; r.__b[6] = e6; r.__b[7] = e7;
    r.__b[8] = e8; r.__b[9] = e9; r.__b[10] = e10; r.__b[11] = e11; r.__b[12] = e12; r.__b[13] = e13; r.__b[14] = e14; r.__b[15] = e15;
    return r;
}
static inline __m128i _mm_setr_epi64(__m64 lo, __m64 hi) { return _mm_set_epi64x(hi.__q, lo.__q); }
static inline __m128i _mm_setr_epi32(int e0, int e1, int e2, int e3) { return _mm_set_epi32(e3, e2, e1, e0); }
static inline __m128i _mm_setr_epi16(short e0, short e1, short e2, short e3, short e4, short e5, short e6, short e7) { return _mm_set_epi16(e7, e6, e5, e4, e3, e2, e1, e0); }
static inline __m128i _mm_setr_epi8(char e0, char e1, char e2, char e3, char e4, char e5, char e6, char e7,
                                    char e8, char e9, char e10, char e11, char e12, char e13, char e14, char e15) {
    return _mm_set_epi8(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0);
}
static inline __m128i _mm_set1_epi64x(long long a) { return _mm_set_epi64x(a, a); }
static inline __m128i _mm_set1_epi64(__m64 a) { return _mm_set_epi64x(a.__q, a.__q); }
static inline __m128i _mm_set1_epi32(int a) { return _mm_set_epi32(a, a, a, a); }
static inline __m128i _mm_set1_epi16(short a) { __m128i r; int i; for (i = 0; i < 8; i++) r.__s[i] = a; return r; }
static inline __m128i _mm_set1_epi8(char a) { __m128i r; memset(&r, a, 16); return r; }
static inline __m128i _mm_cvtsi32_si128(int a) { return _mm_set_epi32(0, 0, 0, a); }
static inline __m128i _mm_cvtsi64_si128(long long a) { return _mm_set_epi64x(0, a); }
#define _mm_cvtsi64x_si128 _mm_cvtsi64_si128
static inline int _mm_cvtsi128_si32(__m128i a) { return a.__i[0]; }
static inline long long _mm_cvtsi128_si64(__m128i a) { return a.__q[0]; }
#define _mm_cvtsi128_si64x _mm_cvtsi128_si64
static inline __m128i _mm_movpi64_epi64(__m64 a) { return _mm_set_epi64x(0, a.__q); }
static inline __m64 _mm_movepi64_pi64(__m128i a) { __m64 r; r.__q = a.__q[0]; return r; }
static inline __m128i _mm_move_epi64(__m128i a) { a.__q[1] = 0; return a; }
static inline __m128i _mm_loadu_si128(const __m128i *p) { __m128i r; memcpy(&r, p, 16); return r; }
static inline __m128i _mm_load_si128(const __m128i *p) { return _mm_loadu_si128(p); }
static inline __m128i _mm_lddqu_si128(const __m128i *p) { return _mm_loadu_si128(p); }
static inline __m128i _mm_loadl_epi64(const __m128i *p) { __m128i r = _mm_setzero_si128(); memcpy(&r, p, 8); return r; }
static inline __m128i _mm_loadu_si64(const void *p) { __m128i r = _mm_setzero_si128(); memcpy(&r, p, 8); return r; }
static inline __m128i _mm_loadu_si32(const void *p) { __m128i r = _mm_setzero_si128(); memcpy(&r, p, 4); return r; }
static inline __m128i _mm_loadu_si16(const void *p) { __m128i r = _mm_setzero_si128(); memcpy(&r, p, 2); return r; }
static inline void _mm_storeu_si128(__m128i *p, __m128i a) { memcpy(p, &a, 16); }
static inline void _mm_store_si128(__m128i *p, __m128i a) { memcpy(p, &a, 16); }
static inline void _mm_storel_epi64(__m128i *p, __m128i a) { memcpy(p, &a, 8); }
static inline void _mm_storeu_si64(void *p, __m128i a) { memcpy(p, &a, 8); }
static inline void _mm_storeu_si32(void *p, __m128i a) { memcpy(p, &a, 4); }
static inline void _mm_storeu_si16(void *p, __m128i a) { memcpy(p, &a, 2); }
static inline void _mm_stream_si128(__m128i *p, __m128i a) { memcpy(p, &a, 16); }
static inline void _mm_stream_si32(int *p, int a) { *p = a; }
static inline void _mm_stream_si64(long long *p, long long a) { *p = a; }
static inline void _mm_maskmoveu_si128(__m128i a, __m128i mask, char *p) { int i; for (i = 0; i < 16; i++) if (mask.__b[i] < 0) p[i] = (char)a.__b[i]; }

#define __MLXCC_MAPI(name, F, n, expr) \
    static inline __m128i name(__m128i a, __m128i b) { __m128i r; int i; for (i = 0; i < n; i++) { __typeof__(a.F[0]) x = a.F[i], y = b.F[i]; (void)y; r.F[i] = (expr); } return r; }
__MLXCC_MAPI(_mm_add_epi8, __ub, 16, (unsigned char)(x + y))
__MLXCC_MAPI(_mm_add_epi16, __us, 8, (unsigned short)(x + y))
__MLXCC_MAPI(_mm_add_epi32, __ui, 4, x + y)
__MLXCC_MAPI(_mm_add_epi64, __uq, 2, x + y)
__MLXCC_MAPI(_mm_adds_epi8, __b, 16, __mlxcc_sat8(x + y))
__MLXCC_MAPI(_mm_adds_epi16, __s, 8, __mlxcc_sat16(x + y))
__MLXCC_MAPI(_mm_adds_epu8, __ub, 16, __mlxcc_satu8(x + y))
__MLXCC_MAPI(_mm_adds_epu16, __us, 8, __mlxcc_satu16(x + y))
__MLXCC_MAPI(_mm_sub_epi8, __ub, 16, (unsigned char)(x - y))
__MLXCC_MAPI(_mm_sub_epi16, __us, 8, (unsigned short)(x - y))
__MLXCC_MAPI(_mm_sub_epi32, __ui, 4, x - y)
__MLXCC_MAPI(_mm_sub_epi64, __uq, 2, x - y)
__MLXCC_MAPI(_mm_subs_epi8, __b, 16, __mlxcc_sat8(x - y))
__MLXCC_MAPI(_mm_subs_epi16, __s, 8, __mlxcc_sat16(x - y))
__MLXCC_MAPI(_mm_subs_epu8, __ub, 16, __mlxcc_satu8(x - y))
__MLXCC_MAPI(_mm_subs_epu16, __us, 8, __mlxcc_satu16(x - y))
__MLXCC_MAPI(_mm_avg_epu8, __ub, 16, (unsigned char)((x + y + 1) >> 1))
__MLXCC_MAPI(_mm_avg_epu16, __us, 8, (unsigned short)(((unsigned)x + y + 1) >> 1))
__MLXCC_MAPI(_mm_max_epi16, __s, 8, x > y ? x : y)
__MLXCC_MAPI(_mm_max_epu8, __ub, 16, x > y ? x : y)
__MLXCC_MAPI(_mm_min_epi16, __s, 8, x < y ? x : y)
__MLXCC_MAPI(_mm_min_epu8, __ub, 16, x < y ? x : y)
__MLXCC_MAPI(_mm_mulhi_epi16, __s, 8, (short)(((int)x * (int)y) >> 16))
__MLXCC_MAPI(_mm_mulhi_epu16, __us, 8, (unsigned short)(((unsigned)x * (unsigned)y) >> 16))
__MLXCC_MAPI(_mm_mullo_epi16, __s, 8, (short)((int)x * (int)y))
__MLXCC_MAPI(_mm_cmpeq_epi8, __b, 16, x == y ? -1 : 0)
__MLXCC_MAPI(_mm_cmpeq_epi16, __s, 8, x == y ? -1 : 0)
__MLXCC_MAPI(_mm_cmpeq_epi32, __i, 4, x == y ? -1 : 0)
__MLXCC_MAPI(_mm_cmpgt_epi8, __b, 16, x > y ? -1 : 0)
__MLXCC_MAPI(_mm_cmpgt_epi16, __s, 8, x > y ? -1 : 0)
__MLXCC_MAPI(_mm_cmpgt_epi32, __i, 4, x > y ? -1 : 0)
__MLXCC_MAPI(_mm_cmplt_epi8, __b, 16, x < y ? -1 : 0)
__MLXCC_MAPI(_mm_cmplt_epi16, __s, 8, x < y ? -1 : 0)
__MLXCC_MAPI(_mm_cmplt_epi32, __i, 4, x < y ? -1 : 0)
static inline __m64 _mm_add_si64(__m64 a, __m64 b) { a.__uq += b.__uq; return a; }
static inline __m64 _mm_sub_si64(__m64 a, __m64 b) { a.__uq -= b.__uq; return a; }
static inline __m128i _mm_madd_epi16(__m128i a, __m128i b) {
    __m128i r; int i;
    for (i = 0; i < 4; i++) r.__i[i] = (int)((unsigned)(a.__s[2 * i] * b.__s[2 * i]) + (unsigned)(a.__s[2 * i + 1] * b.__s[2 * i + 1]));
    return r;
}
static inline __m128i _mm_mul_epu32(__m128i a, __m128i b) {
    __m128i r; r.__uq[0] = (unsigned long long)a.__ui[0] * b.__ui[0]; r.__uq[1] = (unsigned long long)a.__ui[2] * b.__ui[2]; return r;
}
static inline __m64 _mm_mul_su32(__m64 a, __m64 b) { __m64 r; r.__uq = (unsigned long long)a.__ui[0] * b.__ui[0]; return r; }
static inline __m128i _mm_sad_epu8(__m128i a, __m128i b) {
    __m128i r; int h, i;
    for (h = 0; h < 2; h++) {
        unsigned s = 0;
        for (i = 8 * h; i < 8 * h + 8; i++) s += a.__ub[i] > b.__ub[i] ? a.__ub[i] - b.__ub[i] : b.__ub[i] - a.__ub[i];
        r.__uq[h] = s;
    }
    return r;
}
static inline __m128i _mm_and_si128(__m128i a, __m128i b) { a.__q[0] &= b.__q[0]; a.__q[1] &= b.__q[1]; return a; }
static inline __m128i _mm_andnot_si128(__m128i a, __m128i b) { b.__q[0] &= ~a.__q[0]; b.__q[1] &= ~a.__q[1]; return b; }
static inline __m128i _mm_or_si128(__m128i a, __m128i b) { a.__q[0] |= b.__q[0]; a.__q[1] |= b.__q[1]; return a; }
static inline __m128i _mm_xor_si128(__m128i a, __m128i b) { a.__q[0] ^= b.__q[0]; a.__q[1] ^= b.__q[1]; return a; }

#define __MLXCC_SHIFTI(name, iname, F, n, bits, op, arith) \
    static inline __m128i iname(__m128i a, int count) { \
        __m128i r; int i; unsigned c = (unsigned)count; \
        for (i = 0; i < n; i++) r.F[i] = c >= bits ? (arith ? (a.F[i] < 0 ? -1 : 0) : 0) : a.F[i] op c; \
        return r; \
    } \
    static inline __m128i name(__m128i a, __m128i count) { return iname(a, count.__uq[0] > 64 ? 64 : (int)count.__uq[0]); }
__MLXCC_SHIFTI(_mm_sll_epi16, _mm_slli_epi16, __us, 8, 16, <<, 0)
__MLXCC_SHIFTI(_mm_sll_epi32, _mm_slli_epi32, __ui, 4, 32, <<, 0)
__MLXCC_SHIFTI(_mm_sll_epi64, _mm_slli_epi64, __uq, 2, 64, <<, 0)
__MLXCC_SHIFTI(_mm_srl_epi16, _mm_srli_epi16, __us, 8, 16, >>, 0)
__MLXCC_SHIFTI(_mm_srl_epi32, _mm_srli_epi32, __ui, 4, 32, >>, 0)
__MLXCC_SHIFTI(_mm_srl_epi64, _mm_srli_epi64, __uq, 2, 64, >>, 0)
__MLXCC_SHIFTI(_mm_sra_epi16, _mm_srai_epi16, __s, 8, 16, >>, 1)
__MLXCC_SHIFTI(_mm_sra_epi32, _mm_srai_epi32, __i, 4, 32, >>, 1)
/* Byte shifts of the whole register. */
static inline __m128i _mm_slli_si128(__m128i a, int imm) {
    __m128i r = _mm_setzero_si128(); int i; unsigned n = (unsigned)imm;
    for (i = 0; i < 16; i++) if ((unsigned)i >= n) r.__ub[i] = a.__ub[i - n];
    return r;
}
static inline __m128i _mm_srli_si128(__m128i a, int imm) {
    __m128i r = _mm_setzero_si128(); int i; unsigned n = (unsigned)imm;
    for (i = 0; i < 16; i++) if (i + n < 16) r.__ub[i] = a.__ub[i + n];
    return r;
}
#define _mm_bslli_si128 _mm_slli_si128
#define _mm_bsrli_si128 _mm_srli_si128

static inline __m128i _mm_packs_epi16(__m128i a, __m128i b) {
    __m128i r; int i;
    for (i = 0; i < 8; i++) { r.__b[i] = __mlxcc_sat8(a.__s[i]); r.__b[i + 8] = __mlxcc_sat8(b.__s[i]); }
    return r;
}
static inline __m128i _mm_packs_epi32(__m128i a, __m128i b) {
    __m128i r; int i;
    for (i = 0; i < 4; i++) { r.__s[i] = __mlxcc_sat16(a.__i[i]); r.__s[i + 4] = __mlxcc_sat16(b.__i[i]); }
    return r;
}
static inline __m128i _mm_packus_epi16(__m128i a, __m128i b) {
    __m128i r; int i;
    for (i = 0; i < 8; i++) { r.__ub[i] = __mlxcc_satu8(a.__s[i]); r.__ub[i + 8] = __mlxcc_satu8(b.__s[i]); }
    return r;
}
#define __MLXCC_UNPACKI(name, F, n, high) \
    static inline __m128i name(__m128i a, __m128i b) { __m128i r; int i; for (i = 0; i < n / 2; i++) { r.F[2 * i] = a.F[i + (high ? n / 2 : 0)]; r.F[2 * i + 1] = b.F[i + (high ? n / 2 : 0)]; } return r; }
__MLXCC_UNPACKI(_mm_unpacklo_epi8, __ub, 16, 0)
__MLXCC_UNPACKI(_mm_unpacklo_epi16, __us, 8, 0)
__MLXCC_UNPACKI(_mm_unpacklo_epi32, __ui, 4, 0)
__MLXCC_UNPACKI(_mm_unpacklo_epi64, __uq, 2, 0)
__MLXCC_UNPACKI(_mm_unpackhi_epi8, __ub, 16, 1)
__MLXCC_UNPACKI(_mm_unpackhi_epi16, __us, 8, 1)
__MLXCC_UNPACKI(_mm_unpackhi_epi32, __ui, 4, 1)
__MLXCC_UNPACKI(_mm_unpackhi_epi64, __uq, 2, 1)

static inline int _mm_extract_epi16(__m128i a, int imm) { return a.__us[imm & 7]; }
static inline __m128i _mm_insert_epi16(__m128i a, int d, int imm) { a.__s[imm & 7] = (short)d; return a; }
static inline int _mm_movemask_epi8(__m128i a) { int m = 0, i; for (i = 0; i < 16; i++) if (a.__b[i] < 0) m |= 1 << i; return m; }
static inline __m128i _mm_shuffle_epi32(__m128i a, int imm) {
    __m128i r; int i; for (i = 0; i < 4; i++) r.__i[i] = a.__i[(imm >> (2 * i)) & 3]; return r;
}
static inline __m128i _mm_shufflelo_epi16(__m128i a, int imm) {
    __m128i r = a; int i; for (i = 0; i < 4; i++) r.__s[i] = a.__s[(imm >> (2 * i)) & 3]; return r;
}
static inline __m128i _mm_shufflehi_epi16(__m128i a, int imm) {
    __m128i r = a; int i; for (i = 0; i < 4; i++) r.__s[4 + i] = a.__s[4 + ((imm >> (2 * i)) & 3)]; return r;
}

static inline __m128d _mm_cvtepi32_pd(__m128i a) { return _mm_set_pd(a.__i[1], a.__i[0]); }
static inline __m128 _mm_cvtepi32_ps(__m128i a) { return _mm_set_ps((float)a.__i[3], (float)a.__i[2], (float)a.__i[1], (float)a.__i[0]); }
static inline __m128i _mm_cvtpd_epi32(__m128d a) { return _mm_set_epi32(0, 0, __mlxcc_cvt32(__mlxcc_rint(a.__d[1])), __mlxcc_cvt32(__mlxcc_rint(a.__d[0]))); }
static inline __m128i _mm_cvttpd_epi32(__m128d a) { return _mm_set_epi32(0, 0, __mlxcc_cvt32(a.__d[1]), __mlxcc_cvt32(a.__d[0])); }
static inline __m128i _mm_cvtps_epi32(__m128 a) {
    __m128i r; int i; for (i = 0; i < 4; i++) r.__i[i] = __mlxcc_cvt32(__mlxcc_rint(a.__f[i])); return r;
}
static inline __m128i _mm_cvttps_epi32(__m128 a) {
    __m128i r; int i; for (i = 0; i < 4; i++) r.__i[i] = __mlxcc_cvt32(a.__f[i]); return r;
}
#endif
