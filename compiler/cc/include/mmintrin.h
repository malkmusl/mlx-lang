/* <mmintrin.h>: the MMX intrinsics, part of x86-64's baseline with SSE and
   SSE2. The compiler has no vector registers: an __m64 is a union of its
   lanes, each intrinsic a plain function over them with the instruction's
   result (wrapping, saturating, shifting out). Nor has it GCC's vector
   types, which code testing __MMX__, __SSE__ or __SSE2__ takes to come
   with the intrinsics (operators and subscripts on __m128: Skia's
   SkSwizzler_opts.inc): those it does not predefine. */
#ifndef _MLXCC_MMINTRIN_H
#define _MLXCC_MMINTRIN_H

typedef union __attribute__((__aligned__(8))) {
    long long __q;
    unsigned long long __uq;
    unsigned long long __u64[1];
    int __i[2];
    unsigned int __ui[2];
    short __s[4];
    unsigned short __us[4];
    signed char __b[8];
    unsigned char __ub[8];
    float __f[2];
} __m64;

/* Saturation, shared with <emmintrin.h>. */
static inline signed char __mlxcc_sat8(int v) { return (signed char)(v < -128 ? -128 : v > 127 ? 127 : v); }
static inline unsigned char __mlxcc_satu8(int v) { return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); }
static inline short __mlxcc_sat16(int v) { return (short)(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }
static inline unsigned short __mlxcc_satu16(int v) { return (unsigned short)(v < 0 ? 0 : v > 65535 ? 65535 : v); }

static inline void _mm_empty(void) {}
static inline __m64 _mm_cvtsi32_si64(int a) { __m64 r; r.__i[0] = a; r.__i[1] = 0; return r; }
static inline int _mm_cvtsi64_si32(__m64 a) { return a.__i[0]; }
static inline __m64 _mm_cvtsi64_m64(long long a) { __m64 r; r.__q = a; return r; }
static inline long long _mm_cvtm64_si64(__m64 a) { return a.__q; }
static inline __m64 _mm_setzero_si64(void) { __m64 r; r.__q = 0; return r; }
static inline __m64 _mm_set_pi32(int e1, int e0) { __m64 r; r.__i[0] = e0; r.__i[1] = e1; return r; }
static inline __m64 _mm_set_pi16(short e3, short e2, short e1, short e0) { __m64 r; r.__s[0] = e0; r.__s[1] = e1; r.__s[2] = e2; r.__s[3] = e3; return r; }
static inline __m64 _mm_set_pi8(char e7, char e6, char e5, char e4, char e3, char e2, char e1, char e0) {
    __m64 r; r.__b[0] = e0; r.__b[1] = e1; r.__b[2] = e2; r.__b[3] = e3; r.__b[4] = e4; r.__b[5] = e5; r.__b[6] = e6; r.__b[7] = e7; return r;
}
static inline __m64 _mm_setr_pi32(int e0, int e1) { return _mm_set_pi32(e1, e0); }
static inline __m64 _mm_setr_pi16(short e0, short e1, short e2, short e3) { return _mm_set_pi16(e3, e2, e1, e0); }
static inline __m64 _mm_setr_pi8(char e0, char e1, char e2, char e3, char e4, char e5, char e6, char e7) { return _mm_set_pi8(e7, e6, e5, e4, e3, e2, e1, e0); }
static inline __m64 _mm_set1_pi32(int a) { return _mm_set_pi32(a, a); }
static inline __m64 _mm_set1_pi16(short a) { return _mm_set_pi16(a, a, a, a); }
static inline __m64 _mm_set1_pi8(char a) { return _mm_set_pi8(a, a, a, a, a, a, a, a); }

/* Lane by lane: R.F[i] = EXPR over a.F[i] and b.F[i] (as `x` and `y`). */
#define __MLXCC_MAP64(name, F, n, expr) \
    static inline __m64 name(__m64 a, __m64 b) { __m64 r; int i; for (i = 0; i < n; i++) { __typeof__(a.F[0]) x = a.F[i], y = b.F[i]; (void)y; r.F[i] = (expr); } return r; }
__MLXCC_MAP64(_mm_add_pi8, __ub, 8, (unsigned char)(x + y))
__MLXCC_MAP64(_mm_add_pi16, __us, 4, (unsigned short)(x + y))
__MLXCC_MAP64(_mm_add_pi32, __ui, 2, x + y)
__MLXCC_MAP64(_mm_adds_pi8, __b, 8, __mlxcc_sat8(x + y))
__MLXCC_MAP64(_mm_adds_pi16, __s, 4, __mlxcc_sat16(x + y))
__MLXCC_MAP64(_mm_adds_pu8, __ub, 8, __mlxcc_satu8(x + y))
__MLXCC_MAP64(_mm_adds_pu16, __us, 4, __mlxcc_satu16(x + y))
__MLXCC_MAP64(_mm_sub_pi8, __ub, 8, (unsigned char)(x - y))
__MLXCC_MAP64(_mm_sub_pi16, __us, 4, (unsigned short)(x - y))
__MLXCC_MAP64(_mm_sub_pi32, __ui, 2, x - y)
__MLXCC_MAP64(_mm_subs_pi8, __b, 8, __mlxcc_sat8(x - y))
__MLXCC_MAP64(_mm_subs_pi16, __s, 4, __mlxcc_sat16(x - y))
__MLXCC_MAP64(_mm_subs_pu8, __ub, 8, __mlxcc_satu8(x - y))
__MLXCC_MAP64(_mm_subs_pu16, __us, 4, __mlxcc_satu16(x - y))
__MLXCC_MAP64(_mm_mulhi_pi16, __s, 4, (short)(((int)x * (int)y) >> 16))
__MLXCC_MAP64(_mm_mullo_pi16, __s, 4, (short)((int)x * (int)y))
__MLXCC_MAP64(_mm_cmpeq_pi8, __b, 8, x == y ? -1 : 0)
__MLXCC_MAP64(_mm_cmpeq_pi16, __s, 4, x == y ? -1 : 0)
__MLXCC_MAP64(_mm_cmpeq_pi32, __i, 2, x == y ? -1 : 0)
__MLXCC_MAP64(_mm_cmpgt_pi8, __b, 8, x > y ? -1 : 0)
__MLXCC_MAP64(_mm_cmpgt_pi16, __s, 4, x > y ? -1 : 0)
__MLXCC_MAP64(_mm_cmpgt_pi32, __i, 2, x > y ? -1 : 0)

static inline __m64 _mm_madd_pi16(__m64 a, __m64 b) {
    __m64 r; int i;
    for (i = 0; i < 2; i++) r.__i[i] = (int)((unsigned)(a.__s[2 * i] * b.__s[2 * i]) + (unsigned)(a.__s[2 * i + 1] * b.__s[2 * i + 1]));
    return r;
}
static inline __m64 _mm_and_si64(__m64 a, __m64 b) { a.__uq &= b.__uq; return a; }
static inline __m64 _mm_andnot_si64(__m64 a, __m64 b) { b.__uq &= ~a.__uq; return b; }
static inline __m64 _mm_or_si64(__m64 a, __m64 b) { a.__uq |= b.__uq; return a; }
static inline __m64 _mm_xor_si64(__m64 a, __m64 b) { a.__uq ^= b.__uq; return a; }

/* Shifts by a count (the low 64 bits of COUNT, any larger than the lane
   clearing it, or filling it with the sign). */
#define __MLXCC_SHIFT64(name, iname, F, n, bits, op, arith) \
    static inline __m64 iname(__m64 a, int count) { \
        __m64 r; int i; unsigned long long c = (unsigned long long)(unsigned)count; \
        for (i = 0; i < n; i++) r.F[i] = c >= bits ? (arith ? (a.F[i] < 0 ? -1 : 0) : 0) : a.F[i] op c; \
        return r; \
    } \
    static inline __m64 name(__m64 a, __m64 count) { return iname(a, count.__uq > 64 ? 64 : (int)count.__uq); }
__MLXCC_SHIFT64(_mm_sll_pi16, _mm_slli_pi16, __us, 4, 16, <<, 0)
__MLXCC_SHIFT64(_mm_sll_pi32, _mm_slli_pi32, __ui, 2, 32, <<, 0)
__MLXCC_SHIFT64(_mm_sll_si64, _mm_slli_si64, __u64, 1, 64, <<, 0)
__MLXCC_SHIFT64(_mm_srl_pi16, _mm_srli_pi16, __us, 4, 16, >>, 0)
__MLXCC_SHIFT64(_mm_srl_pi32, _mm_srli_pi32, __ui, 2, 32, >>, 0)
__MLXCC_SHIFT64(_mm_srl_si64, _mm_srli_si64, __u64, 1, 64, >>, 0)
__MLXCC_SHIFT64(_mm_sra_pi16, _mm_srai_pi16, __s, 4, 16, >>, 1)
__MLXCC_SHIFT64(_mm_sra_pi32, _mm_srai_pi32, __i, 2, 32, >>, 1)

static inline __m64 _mm_packs_pi16(__m64 a, __m64 b) {
    __m64 r; int i;
    for (i = 0; i < 4; i++) { r.__b[i] = __mlxcc_sat8(a.__s[i]); r.__b[i + 4] = __mlxcc_sat8(b.__s[i]); }
    return r;
}
static inline __m64 _mm_packs_pi32(__m64 a, __m64 b) {
    __m64 r; int i;
    for (i = 0; i < 2; i++) { r.__s[i] = __mlxcc_sat16(a.__i[i]); r.__s[i + 2] = __mlxcc_sat16(b.__i[i]); }
    return r;
}
static inline __m64 _mm_packs_pu16(__m64 a, __m64 b) {
    __m64 r; int i;
    for (i = 0; i < 4; i++) { r.__ub[i] = __mlxcc_satu8(a.__s[i]); r.__ub[i + 4] = __mlxcc_satu8(b.__s[i]); }
    return r;
}
#define __MLXCC_UNPACK64(name, F, n, high) \
    static inline __m64 name(__m64 a, __m64 b) { __m64 r; int i; for (i = 0; i < n / 2; i++) { r.F[2 * i] = a.F[i + (high ? n / 2 : 0)]; r.F[2 * i + 1] = b.F[i + (high ? n / 2 : 0)]; } return r; }
__MLXCC_UNPACK64(_mm_unpacklo_pi8, __ub, 8, 0)
__MLXCC_UNPACK64(_mm_unpacklo_pi16, __us, 4, 0)
__MLXCC_UNPACK64(_mm_unpacklo_pi32, __ui, 2, 0)
__MLXCC_UNPACK64(_mm_unpackhi_pi8, __ub, 8, 1)
__MLXCC_UNPACK64(_mm_unpackhi_pi16, __us, 4, 1)
__MLXCC_UNPACK64(_mm_unpackhi_pi32, __ui, 2, 1)

#define _m_empty _mm_empty
#define _m_from_int _mm_cvtsi32_si64
#define _m_to_int _mm_cvtsi64_si32
#define _m_from_int64 _mm_cvtsi64_m64
#define _m_to_int64 _mm_cvtm64_si64
#define _m_paddb _mm_add_pi8
#define _m_paddw _mm_add_pi16
#define _m_paddd _mm_add_pi32
#define _m_psubb _mm_sub_pi8
#define _m_psubw _mm_sub_pi16
#define _m_psubd _mm_sub_pi32
#define _m_pand _mm_and_si64
#define _m_pandn _mm_andnot_si64
#define _m_por _mm_or_si64
#define _m_pxor _mm_xor_si64
#endif
