/* <xmmintrin.h>: the SSE intrinsics (x86-64's baseline; mlx-cc does not
   predefine __SSE__, having no GCC vector types: <mmintrin.h>), over an
   __m128 that is a union of its lanes (no vector registers), each a plain
   function with the instruction's result: comparisons as all-ones lanes,
   min and max giving the second operand when one is NaN, the conversions
   rounding to nearest even and giving the "integer indefinite" value
   when out of range. The MXCSR is the processor's (stmxcsr, ldmxcsr). */
#ifndef _MLXCC_XMMINTRIN_H
#define _MLXCC_XMMINTRIN_H
#include <mmintrin.h>
#include <mm_malloc.h>

typedef union __attribute__((__aligned__(16))) {
    float __f[4];
    int __i[4];
    unsigned int __ui[4];
    long long __q[2];
} __m128;
typedef __m128 __m128_u;

#define _MM_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))

#define _MM_EXCEPT_INVALID 0x0001
#define _MM_EXCEPT_DENORM 0x0002
#define _MM_EXCEPT_DIV_ZERO 0x0004
#define _MM_EXCEPT_OVERFLOW 0x0008
#define _MM_EXCEPT_UNDERFLOW 0x0010
#define _MM_EXCEPT_INEXACT 0x0020
#define _MM_EXCEPT_MASK 0x003f
#define _MM_MASK_INVALID 0x0080
#define _MM_MASK_DENORM 0x0100
#define _MM_MASK_DIV_ZERO 0x0200
#define _MM_MASK_OVERFLOW 0x0400
#define _MM_MASK_UNDERFLOW 0x0800
#define _MM_MASK_INEXACT 0x1000
#define _MM_MASK_MASK 0x1f80
#define _MM_ROUND_NEAREST 0x0000
#define _MM_ROUND_DOWN 0x2000
#define _MM_ROUND_UP 0x4000
#define _MM_ROUND_TOWARD_ZERO 0x6000
#define _MM_ROUND_MASK 0x6000
#define _MM_FLUSH_ZERO_MASK 0x8000
#define _MM_FLUSH_ZERO_ON 0x8000
#define _MM_FLUSH_ZERO_OFF 0x0000
#define _MM_HINT_ET0 7
#define _MM_HINT_ET1 6
#define _MM_HINT_T0 3
#define _MM_HINT_T1 2
#define _MM_HINT_T2 1
#define _MM_HINT_NTA 0

static inline unsigned int _mm_getcsr(void) { unsigned int r; __asm__ volatile("stmxcsr %0" : "=m"(r)); return r; }
static inline void _mm_setcsr(unsigned int v) { __asm__ volatile("ldmxcsr %0" : : "m"(v)); }
#define _MM_GET_EXCEPTION_STATE() (_mm_getcsr() & _MM_EXCEPT_MASK)
#define _MM_GET_EXCEPTION_MASK() (_mm_getcsr() & _MM_MASK_MASK)
#define _MM_GET_ROUNDING_MODE() (_mm_getcsr() & _MM_ROUND_MASK)
#define _MM_GET_FLUSH_ZERO_MODE() (_mm_getcsr() & _MM_FLUSH_ZERO_MASK)
#define _MM_SET_EXCEPTION_STATE(x) _mm_setcsr((_mm_getcsr() & ~_MM_EXCEPT_MASK) | (x))
#define _MM_SET_EXCEPTION_MASK(x) _mm_setcsr((_mm_getcsr() & ~_MM_MASK_MASK) | (x))
#define _MM_SET_ROUNDING_MODE(x) _mm_setcsr((_mm_getcsr() & ~_MM_ROUND_MASK) | (x))
#define _MM_SET_FLUSH_ZERO_MODE(x) _mm_setcsr((_mm_getcsr() & ~_MM_FLUSH_ZERO_MASK) | (x))

static inline void _mm_sfence(void) { __asm__ volatile("sfence" ::: "memory"); }
static inline void _mm_pause(void) { __asm__ volatile("pause"); }
#define _mm_prefetch(p, i) ((void)(p), (void)(i))

/* Rounding to nearest even (the default rounding mode), and the
   conversions to integers: the "integer indefinite" value (the lowest)
   for NaN and what does not fit. */
static inline double __mlxcc_rint(double x) {
    if (!(x > -4503599627370496.0 && x < 4503599627370496.0)) return x;
    return x >= 0 ? (x + 4503599627370496.0) - 4503599627370496.0 : (x - 4503599627370496.0) + 4503599627370496.0;
}
static inline int __mlxcc_cvt32(double d) { return d > -2147483649.0 && d < 2147483648.0 ? (int)d : (int)0x80000000; }
static inline long long __mlxcc_cvt64(double d) { return d >= -9223372036854775808.0 && d < 9223372036854775808.0 ? (long long)d : (long long)0x8000000000000000LL; }

/* The square roots as the instructions take them (no libm). */
static inline float __mlxcc_sqrtf(float x) { float r; __asm__("sqrtss %1, %%xmm0\n\tmovss %%xmm0, %0" : "=m"(r) : "m"(x) : "xmm0"); return r; }
static inline double __mlxcc_sqrt(double x) { double r; __asm__("sqrtsd %1, %%xmm0\n\tmovsd %%xmm0, %0" : "=m"(r) : "m"(x) : "xmm0"); return r; }

static inline __m128 _mm_setzero_ps(void) { __m128 r; r.__q[0] = 0; r.__q[1] = 0; return r; }
static inline __m128 _mm_undefined_ps(void) { return _mm_setzero_ps(); }
static inline __m128 _mm_set_ss(float w) { __m128 r = _mm_setzero_ps(); r.__f[0] = w; return r; }
static inline __m128 _mm_set_ps(float z, float y, float x, float w) { __m128 r; r.__f[0] = w; r.__f[1] = x; r.__f[2] = y; r.__f[3] = z; return r; }
static inline __m128 _mm_setr_ps(float w, float x, float y, float z) { return _mm_set_ps(z, y, x, w); }
static inline __m128 _mm_set1_ps(float w) { return _mm_set_ps(w, w, w, w); }
#define _mm_set_ps1 _mm_set1_ps
static inline float _mm_cvtss_f32(__m128 a) { return a.__f[0]; }
static inline __m128 _mm_load_ss(const float *p) { return _mm_set_ss(*p); }
static inline __m128 _mm_load1_ps(const float *p) { return _mm_set1_ps(*p); }
#define _mm_load_ps1 _mm_load1_ps
static inline __m128 _mm_loadu_ps(const float *p) { __m128 r; __builtin_memcpy(&r, p, 16); return r; }
static inline __m128 _mm_load_ps(const float *p) { return _mm_loadu_ps(p); }
static inline __m128 _mm_loadr_ps(const float *p) { return _mm_set_ps(p[0], p[1], p[2], p[3]); }
static inline __m128 _mm_loadh_pi(__m128 a, const __m64 *p) { __builtin_memcpy(&a.__f[2], p, 8); return a; }
static inline __m128 _mm_loadl_pi(__m128 a, const __m64 *p) { __builtin_memcpy(&a.__f[0], p, 8); return a; }
static inline void _mm_store_ss(float *p, __m128 a) { *p = a.__f[0]; }
static inline void _mm_storeu_ps(float *p, __m128 a) { __builtin_memcpy(p, &a, 16); }
static inline void _mm_store_ps(float *p, __m128 a) { _mm_storeu_ps(p, a); }
static inline void _mm_store1_ps(float *p, __m128 a) { p[0] = p[1] = p[2] = p[3] = a.__f[0]; }
#define _mm_store_ps1 _mm_store1_ps
static inline void _mm_storer_ps(float *p, __m128 a) { p[0] = a.__f[3]; p[1] = a.__f[2]; p[2] = a.__f[1]; p[3] = a.__f[0]; }
static inline void _mm_storeh_pi(__m64 *p, __m128 a) { __builtin_memcpy(p, &a.__f[2], 8); }
static inline void _mm_storel_pi(__m64 *p, __m128 a) { __builtin_memcpy(p, &a.__f[0], 8); }
static inline void _mm_stream_ps(float *p, __m128 a) { _mm_storeu_ps(p, a); }
static inline void _mm_stream_pi(__m64 *p, __m64 a) { *p = a; }
static inline __m128 _mm_move_ss(__m128 a, __m128 b) { a.__f[0] = b.__f[0]; return a; }

/* Lane by lane over all four lanes (_ps) or the first (_ss, the others
   a's): R.F = EXPR over x and y. */
#define __MLXCC_MAPPS(name, F, expr) \
    static inline __m128 name##_ps(__m128 a, __m128 b) { __m128 r; int i; for (i = 0; i < 4; i++) { float x = a.__f[i], y = b.__f[i]; (void)x; (void)y; r.F[i] = (expr); } return r; } \
    static inline __m128 name##_ss(__m128 a, __m128 b) { float x = a.__f[0], y = b.__f[0]; (void)x; (void)y; a.F[0] = (expr); return a; }
__MLXCC_MAPPS(_mm_add, __f, x + y)
__MLXCC_MAPPS(_mm_sub, __f, x - y)
__MLXCC_MAPPS(_mm_mul, __f, x * y)
__MLXCC_MAPPS(_mm_div, __f, x / y)
__MLXCC_MAPPS(_mm_min, __f, x < y ? x : y)
__MLXCC_MAPPS(_mm_max, __f, x > y ? x : y)
__MLXCC_MAPPS(_mm_cmpeq, __i, x == y ? -1 : 0)
__MLXCC_MAPPS(_mm_cmplt, __i, x < y ? -1 : 0)
__MLXCC_MAPPS(_mm_cmple, __i, x <= y ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpgt, __i, x > y ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpge, __i, x >= y ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpneq, __i, !(x == y) ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpnlt, __i, !(x < y) ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpnle, __i, !(x <= y) ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpngt, __i, !(x > y) ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpnge, __i, !(x >= y) ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpord, __i, (x == x && y == y) ? -1 : 0)
__MLXCC_MAPPS(_mm_cmpunord, __i, (x != x || y != y) ? -1 : 0)
#define __MLXCC_UNARYPS(name, expr) \
    static inline __m128 name##_ps(__m128 a) { int i; for (i = 0; i < 4; i++) { float x = a.__f[i]; a.__f[i] = (expr); } return a; } \
    static inline __m128 name##_ss(__m128 a) { float x = a.__f[0]; a.__f[0] = (expr); return a; }
__MLXCC_UNARYPS(_mm_sqrt, __mlxcc_sqrtf(x))
__MLXCC_UNARYPS(_mm_rcp, 1.0f / x)
__MLXCC_UNARYPS(_mm_rsqrt, 1.0f / __mlxcc_sqrtf(x))

static inline __m128 _mm_and_ps(__m128 a, __m128 b) { a.__q[0] &= b.__q[0]; a.__q[1] &= b.__q[1]; return a; }
static inline __m128 _mm_andnot_ps(__m128 a, __m128 b) { b.__q[0] &= ~a.__q[0]; b.__q[1] &= ~a.__q[1]; return b; }
static inline __m128 _mm_or_ps(__m128 a, __m128 b) { a.__q[0] |= b.__q[0]; a.__q[1] |= b.__q[1]; return a; }
static inline __m128 _mm_xor_ps(__m128 a, __m128 b) { a.__q[0] ^= b.__q[0]; a.__q[1] ^= b.__q[1]; return a; }

/* comiss and ucomiss: false when either is NaN (but neq, true). */
#define __MLXCC_COMISS(op, expr) \
    static inline int _mm_comi##op##_ss(__m128 a, __m128 b) { float x = a.__f[0], y = b.__f[0]; return (expr); } \
    static inline int _mm_ucomi##op##_ss(__m128 a, __m128 b) { float x = a.__f[0], y = b.__f[0]; return (expr); }
__MLXCC_COMISS(eq, x == y)
__MLXCC_COMISS(lt, x < y)
__MLXCC_COMISS(le, x <= y)
__MLXCC_COMISS(gt, x > y)
__MLXCC_COMISS(ge, x >= y)
__MLXCC_COMISS(neq, !(x == y))

static inline int _mm_cvtss_si32(__m128 a) { return __mlxcc_cvt32(__mlxcc_rint(a.__f[0])); }
#define _mm_cvt_ss2si _mm_cvtss_si32
static inline long long _mm_cvtss_si64(__m128 a) { return __mlxcc_cvt64(__mlxcc_rint(a.__f[0])); }
static inline int _mm_cvttss_si32(__m128 a) { return __mlxcc_cvt32(a.__f[0]); }
#define _mm_cvtt_ss2si _mm_cvttss_si32
static inline long long _mm_cvttss_si64(__m128 a) { return __mlxcc_cvt64(a.__f[0]); }
#define _mm_cvttss_si64x _mm_cvttss_si64
#define _mm_cvtss_si64x _mm_cvtss_si64
static inline __m128 _mm_cvtsi32_ss(__m128 a, int b) { a.__f[0] = (float)b; return a; }
#define _mm_cvt_si2ss _mm_cvtsi32_ss
static inline __m128 _mm_cvtsi64_ss(__m128 a, long long b) { a.__f[0] = (float)b; return a; }
#define _mm_cvtsi64x_ss _mm_cvtsi64_ss
static inline __m64 _mm_cvtps_pi32(__m128 a) { __m64 r; r.__i[0] = __mlxcc_cvt32(__mlxcc_rint(a.__f[0])); r.__i[1] = __mlxcc_cvt32(__mlxcc_rint(a.__f[1])); return r; }
#define _mm_cvt_ps2pi _mm_cvtps_pi32
static inline __m64 _mm_cvttps_pi32(__m128 a) { __m64 r; r.__i[0] = __mlxcc_cvt32(a.__f[0]); r.__i[1] = __mlxcc_cvt32(a.__f[1]); return r; }
#define _mm_cvtt_ps2pi _mm_cvttps_pi32
static inline __m128 _mm_cvtpi32_ps(__m128 a, __m64 b) { a.__f[0] = (float)b.__i[0]; a.__f[1] = (float)b.__i[1]; return a; }
#define _mm_cvt_pi2ps _mm_cvtpi32_ps
static inline __m128 _mm_cvtpi16_ps(__m64 a) { return _mm_set_ps(a.__s[3], a.__s[2], a.__s[1], a.__s[0]); }
static inline __m128 _mm_cvtpu16_ps(__m64 a) { return _mm_set_ps(a.__us[3], a.__us[2], a.__us[1], a.__us[0]); }
static inline __m128 _mm_cvtpi8_ps(__m64 a) { return _mm_set_ps(a.__b[3], a.__b[2], a.__b[1], a.__b[0]); }
static inline __m128 _mm_cvtpu8_ps(__m64 a) { return _mm_set_ps(a.__ub[3], a.__ub[2], a.__ub[1], a.__ub[0]); }
static inline __m128 _mm_cvtpi32x2_ps(__m64 a, __m64 b) { return _mm_set_ps((float)b.__i[1], (float)b.__i[0], (float)a.__i[1], (float)a.__i[0]); }
static inline __m64 _mm_cvtps_pi16(__m128 a) {
    __m64 r; int i;
    for (i = 0; i < 4; i++) { double d = __mlxcc_rint(a.__f[i]); r.__s[i] = d >= 32767.0 ? 32767 : d <= -32768.0 ? -32768 : d == d ? (short)d : -32768; }
    return r;
}
static inline __m64 _mm_cvtps_pi8(__m128 a) {
    __m64 r; int i;
    r.__q = 0;
    for (i = 0; i < 4; i++) { double d = __mlxcc_rint(a.__f[i]); r.__b[i] = d >= 127.0 ? 127 : d <= -128.0 ? -128 : d == d ? (signed char)d : -128; }
    return r;
}

static inline __m128 _mm_shuffle_ps(__m128 a, __m128 b, int imm) {
    __m128 r; r.__f[0] = a.__f[imm & 3]; r.__f[1] = a.__f[(imm >> 2) & 3]; r.__f[2] = b.__f[(imm >> 4) & 3]; r.__f[3] = b.__f[(imm >> 6) & 3]; return r;
}
static inline __m128 _mm_unpackhi_ps(__m128 a, __m128 b) { return _mm_set_ps(b.__f[3], a.__f[3], b.__f[2], a.__f[2]); }
static inline __m128 _mm_unpacklo_ps(__m128 a, __m128 b) { return _mm_set_ps(b.__f[1], a.__f[1], b.__f[0], a.__f[0]); }
static inline __m128 _mm_movehl_ps(__m128 a, __m128 b) { return _mm_set_ps(a.__f[3], a.__f[2], b.__f[3], b.__f[2]); }
static inline __m128 _mm_movelh_ps(__m128 a, __m128 b) { return _mm_set_ps(b.__f[1], b.__f[0], a.__f[1], a.__f[0]); }
static inline int _mm_movemask_ps(__m128 a) { int m = 0, i; for (i = 0; i < 4; i++) if (a.__i[i] < 0) m |= 1 << i; return m; }

/* The MMX extensions SSE brought. */
static inline int _mm_extract_pi16(__m64 a, int imm) { return a.__us[imm & 3]; }
static inline __m64 _mm_insert_pi16(__m64 a, int d, int imm) { a.__s[imm & 3] = (short)d; return a; }
__MLXCC_MAP64(_mm_max_pi16, __s, 4, x > y ? x : y)
__MLXCC_MAP64(_mm_max_pu8, __ub, 8, x > y ? x : y)
__MLXCC_MAP64(_mm_min_pi16, __s, 4, x < y ? x : y)
__MLXCC_MAP64(_mm_min_pu8, __ub, 8, x < y ? x : y)
__MLXCC_MAP64(_mm_mulhi_pu16, __us, 4, (unsigned short)(((unsigned)x * (unsigned)y) >> 16))
__MLXCC_MAP64(_mm_avg_pu8, __ub, 8, (unsigned char)((x + y + 1) >> 1))
__MLXCC_MAP64(_mm_avg_pu16, __us, 4, (unsigned short)(((unsigned)x + y + 1) >> 1))
static inline int _mm_movemask_pi8(__m64 a) { int m = 0, i; for (i = 0; i < 8; i++) if (a.__b[i] < 0) m |= 1 << i; return m; }
static inline __m64 _mm_shuffle_pi16(__m64 a, int imm) {
    __m64 r; int i; for (i = 0; i < 4; i++) r.__s[i] = a.__s[(imm >> (2 * i)) & 3]; return r;
}
static inline __m64 _mm_sad_pu8(__m64 a, __m64 b) {
    __m64 r; int i; unsigned s = 0;
    for (i = 0; i < 8; i++) s += a.__ub[i] > b.__ub[i] ? a.__ub[i] - b.__ub[i] : b.__ub[i] - a.__ub[i];
    r.__q = s; return r;
}
static inline void _mm_maskmove_si64(__m64 a, __m64 mask, char *p) { int i; for (i = 0; i < 8; i++) if (mask.__b[i] < 0) p[i] = (char)a.__b[i]; }
#define _m_pextrw _mm_extract_pi16
#define _m_pinsrw _mm_insert_pi16
#define _m_pmaxsw _mm_max_pi16
#define _m_pmaxub _mm_max_pu8
#define _m_pminsw _mm_min_pi16
#define _m_pminub _mm_min_pu8
#define _m_pmovmskb _mm_movemask_pi8
#define _m_pmulhuw _mm_mulhi_pu16
#define _m_pshufw _mm_shuffle_pi16
#define _m_pavgb _mm_avg_pu8
#define _m_pavgw _mm_avg_pu16
#define _m_psadbw _mm_sad_pu8
#define _m_maskmovq _mm_maskmove_si64

#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3) do { \
        __m128 __t0 = _mm_unpacklo_ps((row0), (row1)), __t1 = _mm_unpacklo_ps((row2), (row3)); \
        __m128 __t2 = _mm_unpackhi_ps((row0), (row1)), __t3 = _mm_unpackhi_ps((row2), (row3)); \
        (row0) = _mm_movelh_ps(__t0, __t1); (row1) = _mm_movehl_ps(__t1, __t0); \
        (row2) = _mm_movelh_ps(__t2, __t3); (row3) = _mm_movehl_ps(__t3, __t2); \
    } while (0)
#endif
