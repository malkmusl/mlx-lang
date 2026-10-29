/* <emmintrin.h>: the SSE2 intrinsics, all of them (x86-64's baseline:
   SIMDe's native x86 path, which WebKit's SIMD helpers take on x86-64,
   uses them; mlx-cc does not predefine __SSE2__, having no GCC vector
   types: <mmintrin.h>), over __m128d and __m128i that are unions of their
   lanes (the compiler has no vector registers). Each is its instruction
   (__builtin_mlxcc_asm: the operands' bytes loaded into xmm0 and xmm1,
   the instruction, the result stored; inlined, that is all a call is),
   those with an immediate macros of a statement expression, the ones
   that set lanes from scalars plain stores. <xmmintrin.h> and
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
static inline __m128 _mm_castpd_ps(__m128d a) { return *(__m128 *)&a; }
static inline __m128i _mm_castpd_si128(__m128d a) { return *(__m128i *)&a; }
static inline __m128d _mm_castps_pd(__m128 a) { return *(__m128d *)&a; }
static inline __m128i _mm_castps_si128(__m128 a) { return *(__m128i *)&a; }
static inline __m128 _mm_castsi128_ps(__m128i a) { return *(__m128 *)&a; }
static inline __m128d _mm_castsi128_pd(__m128i a) { return *(__m128d *)&a; }

/* Doubles. */
static inline __m128d _mm_setzero_pd(void) { __m128d r; r.__q[0] = 0; r.__q[1] = 0; return r; }
static inline __m128d _mm_undefined_pd(void) { return _mm_setzero_pd(); }
static inline __m128d _mm_set_sd(double w) { __m128d r; r.__d[0] = w; r.__d[1] = 0.0; return r; }
static inline __m128d _mm_set_pd(double hi, double lo) { __m128d r; r.__d[0] = lo; r.__d[1] = hi; return r; }
static inline __m128d _mm_setr_pd(double lo, double hi) { return _mm_set_pd(hi, lo); }
static inline __m128d _mm_set1_pd(double w) { __m128d r; __builtin_mlxcc_asm("movsd (%rsi), %xmm0\nunpcklpd %xmm0, %xmm0\nmovupd %xmm0, (%rdi)", &r, &w); return r; }
#define _mm_set_pd1 _mm_set1_pd
static inline double _mm_cvtsd_f64(__m128d a) { return a.__d[0]; }
static inline __m128d _mm_loadu_pd(const double *p) { __m128d r; __builtin_mlxcc_asm("movupd (%rsi), %xmm0\nmovupd %xmm0, (%rdi)", &r, p); return r; }
static inline __m128d _mm_load_pd(const double *p) { return _mm_loadu_pd(p); }
static inline __m128d _mm_load_sd(const double *p) { __m128d r; __builtin_mlxcc_asm("movsd (%rsi), %xmm0\nmovupd %xmm0, (%rdi)", &r, p); return r; }
static inline __m128d _mm_load1_pd(const double *p) { __m128d r; __builtin_mlxcc_asm("movsd (%rsi), %xmm0\nunpcklpd %xmm0, %xmm0\nmovupd %xmm0, (%rdi)", &r, p); return r; }
#define _mm_load_pd1 _mm_load1_pd
static inline __m128d _mm_loadr_pd(const double *p) { __m128d r; __builtin_mlxcc_asm("movupd (%rsi), %xmm0\nshufpd $1, %xmm0, %xmm0\nmovupd %xmm0, (%rdi)", &r, p); return r; }
static inline __m128d _mm_loadh_pd(__m128d a, const double *p) { __builtin_mlxcc_asm("movupd (%rdi), %xmm0\nmovhpd (%rsi), %xmm0\nmovupd %xmm0, (%rdi)", &a, p); return a; }
static inline __m128d _mm_loadl_pd(__m128d a, const double *p) { __builtin_mlxcc_asm("movupd (%rdi), %xmm0\nmovlpd (%rsi), %xmm0\nmovupd %xmm0, (%rdi)", &a, p); return a; }
static inline void _mm_storeu_pd(double *p, __m128d a) { __builtin_mlxcc_asm("movupd (%rsi), %xmm0\nmovupd %xmm0, (%rdi)", p, &a); }
static inline void _mm_store_pd(double *p, __m128d a) { _mm_storeu_pd(p, a); }
static inline void _mm_store_sd(double *p, __m128d a) { *p = a.__d[0]; }
static inline void _mm_store1_pd(double *p, __m128d a) { p[0] = p[1] = a.__d[0]; }
#define _mm_store_pd1 _mm_store1_pd
static inline void _mm_storer_pd(double *p, __m128d a) { p[0] = a.__d[1]; p[1] = a.__d[0]; }
static inline void _mm_storeh_pd(double *p, __m128d a) { *p = a.__d[1]; }
static inline void _mm_storel_pd(double *p, __m128d a) { *p = a.__d[0]; }
static inline void _mm_stream_pd(double *p, __m128d a) { _mm_storeu_pd(p, a); }
static inline __m128d _mm_move_sd(__m128d a, __m128d b) { __builtin_mlxcc_asm("movupd (%rdi), %xmm0\nmovupd (%rsi), %xmm1\nmovsd %xmm1, %xmm0\nmovupd %xmm0, (%rdi)", &a, &b); return a; }

__MLXCC_BIN(_mm_add_pd, __m128d, "addpd")
__MLXCC_BIN(_mm_add_sd, __m128d, "addsd")
__MLXCC_BIN(_mm_sub_pd, __m128d, "subpd")
__MLXCC_BIN(_mm_sub_sd, __m128d, "subsd")
__MLXCC_BIN(_mm_mul_pd, __m128d, "mulpd")
__MLXCC_BIN(_mm_mul_sd, __m128d, "mulsd")
__MLXCC_BIN(_mm_div_pd, __m128d, "divpd")
__MLXCC_BIN(_mm_div_sd, __m128d, "divsd")
__MLXCC_BIN(_mm_min_pd, __m128d, "minpd")
__MLXCC_BIN(_mm_min_sd, __m128d, "minsd")
__MLXCC_BIN(_mm_max_pd, __m128d, "maxpd")
__MLXCC_BIN(_mm_max_sd, __m128d, "maxsd")
__MLXCC_BIN(_mm_cmpeq_pd, __m128d, "cmpeqpd")
__MLXCC_BIN(_mm_cmplt_pd, __m128d, "cmpltpd")
__MLXCC_BIN(_mm_cmple_pd, __m128d, "cmplepd")
__MLXCC_BINR(_mm_cmpgt_pd, __m128d, "cmpltpd")
__MLXCC_BINR(_mm_cmpge_pd, __m128d, "cmplepd")
__MLXCC_BIN(_mm_cmpneq_pd, __m128d, "cmpneqpd")
__MLXCC_BIN(_mm_cmpnlt_pd, __m128d, "cmpnltpd")
__MLXCC_BIN(_mm_cmpnle_pd, __m128d, "cmpnlepd")
__MLXCC_BINR(_mm_cmpngt_pd, __m128d, "cmpnltpd")
__MLXCC_BINR(_mm_cmpnge_pd, __m128d, "cmpnlepd")
__MLXCC_BIN(_mm_cmpord_pd, __m128d, "cmpordpd")
__MLXCC_BIN(_mm_cmpunord_pd, __m128d, "cmpunordpd")
#define __MLXCC_CMPSD(name, expr) \
    static inline __m128d name##_sd(__m128d a, __m128d b) { double x = a.__d[0], y = b.__d[0]; a.__q[0] = (expr) ? -1 : 0; return a; }
__MLXCC_CMPSD(_mm_cmpeq, x == y)
__MLXCC_CMPSD(_mm_cmplt, x < y)
__MLXCC_CMPSD(_mm_cmple, x <= y)
__MLXCC_CMPSD(_mm_cmpgt, x > y)
__MLXCC_CMPSD(_mm_cmpge, x >= y)
__MLXCC_CMPSD(_mm_cmpneq, !(x == y))
__MLXCC_CMPSD(_mm_cmpnlt, !(x < y))
__MLXCC_CMPSD(_mm_cmpnle, !(x <= y))
__MLXCC_CMPSD(_mm_cmpngt, !(x > y))
__MLXCC_CMPSD(_mm_cmpnge, !(x >= y))
__MLXCC_CMPSD(_mm_cmpord, x == x && y == y)
__MLXCC_CMPSD(_mm_cmpunord, x != x || y != y)
__MLXCC_UNARY(_mm_sqrt_pd, __m128d, "sqrtpd")
__MLXCC_BIN(_mm_sqrt_sd, __m128d, "sqrtsd")
__MLXCC_BIN(_mm_and_pd, __m128d, "andpd")
__MLXCC_BIN(_mm_andnot_pd, __m128d, "andnpd")
__MLXCC_BIN(_mm_or_pd, __m128d, "orpd")
__MLXCC_BIN(_mm_xor_pd, __m128d, "xorpd")
#define __MLXCC_COMISD(op, expr) \
    static inline int _mm_comi##op##_sd(__m128d a, __m128d b) { double x = a.__d[0], y = b.__d[0]; return (expr); } \
    static inline int _mm_ucomi##op##_sd(__m128d a, __m128d b) { double x = a.__d[0], y = b.__d[0]; return (expr); }
__MLXCC_COMISD(eq, x == y)
__MLXCC_COMISD(lt, x < y)
__MLXCC_COMISD(le, x <= y)
__MLXCC_COMISD(gt, x > y)
__MLXCC_COMISD(ge, x >= y)
__MLXCC_COMISD(neq, !(x == y))
#define _mm_shuffle_pd(a, b, imm) ({ __m128d __a = (a), __b = (b); __builtin_mlxcc_asmi("movupd (%rdi), %xmm0\nmovupd (%rsi), %xmm1\nshufpd $%i, %xmm1, %xmm0\nmovupd %xmm0, (%rdi)", (imm), &__a, &__b); __a; })
__MLXCC_BIN(_mm_unpackhi_pd, __m128d, "unpckhpd")
__MLXCC_BIN(_mm_unpacklo_pd, __m128d, "unpcklpd")
__MLXCC_MASK(_mm_movemask_pd, __m128d, "movmskpd")

/* Conversions. */
static inline int _mm_cvtsd_si32(__m128d a) { return (int)__builtin_mlxcc_asm("cvtsd2si (%rdi), %eax", &a); }
static inline long long _mm_cvtsd_si64(__m128d a) { return __builtin_mlxcc_asm("cvtsd2si (%rdi), %rax", &a); }
#define _mm_cvtsd_si64x _mm_cvtsd_si64
static inline int _mm_cvttsd_si32(__m128d a) { return (int)__builtin_mlxcc_asm("cvttsd2si (%rdi), %eax", &a); }
static inline long long _mm_cvttsd_si64(__m128d a) { return __builtin_mlxcc_asm("cvttsd2si (%rdi), %rax", &a); }
#define _mm_cvttsd_si64x _mm_cvttsd_si64
static inline __m128d _mm_cvtsi32_sd(__m128d a, int b) { __builtin_mlxcc_asm("movupd (%rdi), %xmm0\ncvtsi2sd %esi, %xmm0\nmovupd %xmm0, (%rdi)", &a, b); return a; }
static inline __m128d _mm_cvtsi64_sd(__m128d a, long long b) { __builtin_mlxcc_asm("movupd (%rdi), %xmm0\ncvtsi2sdq %rsi, %xmm0\nmovupd %xmm0, (%rdi)", &a, b); return a; }
#define _mm_cvtsi64x_sd _mm_cvtsi64_sd
static inline __m128 _mm_cvtsd_ss(__m128 a, __m128d b) { __builtin_mlxcc_asm("movups (%rdi), %xmm0\nmovupd (%rsi), %xmm1\ncvtsd2ss %xmm1, %xmm0\nmovups %xmm0, (%rdi)", &a, &b); return a; }
static inline __m128d _mm_cvtss_sd(__m128d a, __m128 b) { __builtin_mlxcc_asm("movupd (%rdi), %xmm0\nmovups (%rsi), %xmm1\ncvtss2sd %xmm1, %xmm0\nmovupd %xmm0, (%rdi)", &a, &b); return a; }
__MLXCC_CONVERT(_mm_cvtps_pd, __m128, __m128d, "cvtps2pd")
__MLXCC_CONVERT(_mm_cvtpd_ps, __m128d, __m128, "cvtpd2ps")
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
static inline __m128i _mm_set1_epi64x(long long a) { __m128i r; __builtin_mlxcc_asm("movq %rsi, %xmm0\npunpcklqdq %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", &r, a); return r; }
static inline __m128i _mm_set1_epi64(__m64 a) { return _mm_set1_epi64x(a.__q); }
static inline __m128i _mm_set1_epi32(int a) { __m128i r; __builtin_mlxcc_asm("movd %esi, %xmm0\npshufd $0, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", &r, a); return r; }
static inline __m128i _mm_set1_epi16(short a) { __m128i r; __builtin_mlxcc_asm("movd %esi, %xmm0\npunpcklwd %xmm0, %xmm0\npshufd $0, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", &r, a); return r; }
static inline __m128i _mm_set1_epi8(char a) { __m128i r; __builtin_mlxcc_asm("movd %esi, %xmm0\npunpcklbw %xmm0, %xmm0\npunpcklwd %xmm0, %xmm0\npshufd $0, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", &r, a); return r; }
static inline __m128i _mm_cvtsi32_si128(int a) { __m128i r; __builtin_mlxcc_asm("movd %esi, %xmm0\nmovdqu %xmm0, (%rdi)", &r, a); return r; }
static inline __m128i _mm_cvtsi64_si128(long long a) { __m128i r; __builtin_mlxcc_asm("movq %rsi, %xmm0\nmovdqu %xmm0, (%rdi)", &r, a); return r; }
#define _mm_cvtsi64x_si128 _mm_cvtsi64_si128
static inline int _mm_cvtsi128_si32(__m128i a) { return a.__i[0]; }
static inline long long _mm_cvtsi128_si64(__m128i a) { return a.__q[0]; }
#define _mm_cvtsi128_si64x _mm_cvtsi128_si64
static inline __m128i _mm_movpi64_epi64(__m64 a) { return _mm_set_epi64x(0, a.__q); }
static inline __m64 _mm_movepi64_pi64(__m128i a) { __m64 r; r.__q = a.__q[0]; return r; }
static inline __m128i _mm_move_epi64(__m128i a) { a.__q[1] = 0; return a; }
static inline __m128i _mm_loadu_si128(const __m128i *p) { __m128i r; __builtin_mlxcc_asm("movdqu (%rsi), %xmm0\nmovdqu %xmm0, (%rdi)", &r, p); return r; }
static inline __m128i _mm_load_si128(const __m128i *p) { return _mm_loadu_si128(p); }
static inline __m128i _mm_lddqu_si128(const __m128i *p) { return _mm_loadu_si128(p); }
static inline __m128i _mm_loadl_epi64(const __m128i *p) { __m128i r; __builtin_mlxcc_asm("movq (%rsi), %xmm0\nmovdqu %xmm0, (%rdi)", &r, p); return r; }
static inline __m128i _mm_loadu_si64(const void *p) { __m128i r; __builtin_mlxcc_asm("movq (%rsi), %xmm0\nmovdqu %xmm0, (%rdi)", &r, p); return r; }
static inline __m128i _mm_loadu_si32(const void *p) { __m128i r; __builtin_mlxcc_asm("movd (%rsi), %xmm0\nmovdqu %xmm0, (%rdi)", &r, p); return r; }
static inline __m128i _mm_loadu_si16(const void *p) { __m128i r = _mm_setzero_si128(); memcpy(&r, p, 2); return r; }
static inline void _mm_storeu_si128(__m128i *p, __m128i a) { __builtin_mlxcc_asm("movdqu (%rsi), %xmm0\nmovdqu %xmm0, (%rdi)", p, &a); }
static inline void _mm_store_si128(__m128i *p, __m128i a) { _mm_storeu_si128(p, a); }
static inline void _mm_storel_epi64(__m128i *p, __m128i a) { __builtin_mlxcc_asm("movq (%rsi), %xmm0\nmovq %xmm0, (%rdi)", p, &a); }
static inline void _mm_storeu_si64(void *p, __m128i a) { __builtin_mlxcc_asm("movq (%rsi), %xmm0\nmovq %xmm0, (%rdi)", p, &a); }
static inline void _mm_storeu_si32(void *p, __m128i a) { __builtin_mlxcc_asm("movd (%rsi), %xmm0\nmovd %xmm0, (%rdi)", p, &a); }
static inline void _mm_storeu_si16(void *p, __m128i a) { memcpy(p, &a, 2); }
static inline void _mm_stream_si128(__m128i *p, __m128i a) { _mm_storeu_si128(p, a); }
static inline void _mm_stream_si32(int *p, int a) { *p = a; }
static inline void _mm_stream_si64(long long *p, long long a) { *p = a; }
static inline void _mm_maskmoveu_si128(__m128i a, __m128i mask, char *p) { int i; for (i = 0; i < 16; i++) if (mask.__b[i] < 0) p[i] = (char)a.__b[i]; }

__MLXCC_BIN(_mm_add_epi8, __m128i, "paddb")
__MLXCC_BIN(_mm_add_epi16, __m128i, "paddw")
__MLXCC_BIN(_mm_add_epi32, __m128i, "paddd")
__MLXCC_BIN(_mm_add_epi64, __m128i, "paddq")
__MLXCC_BIN(_mm_adds_epi8, __m128i, "paddsb")
__MLXCC_BIN(_mm_adds_epi16, __m128i, "paddsw")
__MLXCC_BIN(_mm_adds_epu8, __m128i, "paddusb")
__MLXCC_BIN(_mm_adds_epu16, __m128i, "paddusw")
__MLXCC_BIN(_mm_sub_epi8, __m128i, "psubb")
__MLXCC_BIN(_mm_sub_epi16, __m128i, "psubw")
__MLXCC_BIN(_mm_sub_epi32, __m128i, "psubd")
__MLXCC_BIN(_mm_sub_epi64, __m128i, "psubq")
__MLXCC_BIN(_mm_subs_epi8, __m128i, "psubsb")
__MLXCC_BIN(_mm_subs_epi16, __m128i, "psubsw")
__MLXCC_BIN(_mm_subs_epu8, __m128i, "psubusb")
__MLXCC_BIN(_mm_subs_epu16, __m128i, "psubusw")
__MLXCC_BIN(_mm_avg_epu8, __m128i, "pavgb")
__MLXCC_BIN(_mm_avg_epu16, __m128i, "pavgw")
__MLXCC_BIN(_mm_max_epi16, __m128i, "pmaxsw")
__MLXCC_BIN(_mm_max_epu8, __m128i, "pmaxub")
__MLXCC_BIN(_mm_min_epi16, __m128i, "pminsw")
__MLXCC_BIN(_mm_min_epu8, __m128i, "pminub")
__MLXCC_BIN(_mm_mulhi_epi16, __m128i, "pmulhw")
__MLXCC_BIN(_mm_mulhi_epu16, __m128i, "pmulhuw")
__MLXCC_BIN(_mm_mullo_epi16, __m128i, "pmullw")
__MLXCC_BIN(_mm_cmpeq_epi8, __m128i, "pcmpeqb")
__MLXCC_BIN(_mm_cmpeq_epi16, __m128i, "pcmpeqw")
__MLXCC_BIN(_mm_cmpeq_epi32, __m128i, "pcmpeqd")
__MLXCC_BIN(_mm_cmpgt_epi8, __m128i, "pcmpgtb")
__MLXCC_BIN(_mm_cmpgt_epi16, __m128i, "pcmpgtw")
__MLXCC_BIN(_mm_cmpgt_epi32, __m128i, "pcmpgtd")
__MLXCC_BINR(_mm_cmplt_epi8, __m128i, "pcmpgtb")
__MLXCC_BINR(_mm_cmplt_epi16, __m128i, "pcmpgtw")
__MLXCC_BINR(_mm_cmplt_epi32, __m128i, "pcmpgtd")
static inline __m64 _mm_add_si64(__m64 a, __m64 b) { a.__uq += b.__uq; return a; }
static inline __m64 _mm_sub_si64(__m64 a, __m64 b) { a.__uq -= b.__uq; return a; }
__MLXCC_BIN(_mm_madd_epi16, __m128i, "pmaddwd")
__MLXCC_BIN(_mm_mul_epu32, __m128i, "pmuludq")
static inline __m64 _mm_mul_su32(__m64 a, __m64 b) { __m64 r; r.__uq = (unsigned long long)a.__ui[0] * b.__ui[0]; return r; }
__MLXCC_BIN(_mm_sad_epu8, __m128i, "psadbw")
__MLXCC_BIN(_mm_and_si128, __m128i, "pand")
__MLXCC_BIN(_mm_andnot_si128, __m128i, "pandn")
__MLXCC_BIN(_mm_or_si128, __m128i, "por")
__MLXCC_BIN(_mm_xor_si128, __m128i, "pxor")

/* By a count in the vector's low quadword (a count past the lane's
   width clears it, or fills it with the sign), or in an int, which the
   instruction takes from xmm1 alike. */
#define __MLXCC_SHIFT(name, iname, insn) \
    __MLXCC_BIN(name, __m128i, insn) \
    static inline __m128i iname(__m128i a, int count) { __builtin_mlxcc_asm("movdqu (%rdi), %xmm0\nmovd %esi, %xmm1\n" insn " %xmm1, %xmm0\nmovdqu %xmm0, (%rdi)", &a, count); return a; }
__MLXCC_SHIFT(_mm_sll_epi16, _mm_slli_epi16, "psllw")
__MLXCC_SHIFT(_mm_sll_epi32, _mm_slli_epi32, "pslld")
__MLXCC_SHIFT(_mm_sll_epi64, _mm_slli_epi64, "psllq")
__MLXCC_SHIFT(_mm_srl_epi16, _mm_srli_epi16, "psrlw")
__MLXCC_SHIFT(_mm_srl_epi32, _mm_srli_epi32, "psrld")
__MLXCC_SHIFT(_mm_srl_epi64, _mm_srli_epi64, "psrlq")
__MLXCC_SHIFT(_mm_sra_epi16, _mm_srai_epi16, "psraw")
__MLXCC_SHIFT(_mm_sra_epi32, _mm_srai_epi32, "psrad")
/* Byte shifts of the whole register, by a constant. */
#define _mm_slli_si128(a, imm) ({ __m128i __a = (a); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npslldq $%i, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a); __a; })
#define _mm_srli_si128(a, imm) ({ __m128i __a = (a); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npsrldq $%i, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a); __a; })
#define _mm_bslli_si128 _mm_slli_si128
#define _mm_bsrli_si128 _mm_srli_si128

__MLXCC_BIN(_mm_packs_epi16, __m128i, "packsswb")
__MLXCC_BIN(_mm_packs_epi32, __m128i, "packssdw")
__MLXCC_BIN(_mm_packus_epi16, __m128i, "packuswb")
__MLXCC_BIN(_mm_unpacklo_epi8, __m128i, "punpcklbw")
__MLXCC_BIN(_mm_unpacklo_epi16, __m128i, "punpcklwd")
__MLXCC_BIN(_mm_unpacklo_epi32, __m128i, "punpckldq")
__MLXCC_BIN(_mm_unpacklo_epi64, __m128i, "punpcklqdq")
__MLXCC_BIN(_mm_unpackhi_epi8, __m128i, "punpckhbw")
__MLXCC_BIN(_mm_unpackhi_epi16, __m128i, "punpckhwd")
__MLXCC_BIN(_mm_unpackhi_epi32, __m128i, "punpckhdq")
__MLXCC_BIN(_mm_unpackhi_epi64, __m128i, "punpckhqdq")

#define _mm_extract_epi16(a, imm) ({ __m128i __a = (a); (int)__builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npextrw $%i, %xmm0, %eax", (imm), &__a); })
#define _mm_insert_epi16(a, d, imm) ({ __m128i __a = (a); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npinsrw $%i, %esi, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a, (d)); __a; })
__MLXCC_MASK(_mm_movemask_epi8, __m128i, "pmovmskb")
#define _mm_shuffle_epi32(a, imm) ({ __m128i __a = (a); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npshufd $%i, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a); __a; })
#define _mm_shufflelo_epi16(a, imm) ({ __m128i __a = (a); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npshuflw $%i, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a); __a; })
#define _mm_shufflehi_epi16(a, imm) ({ __m128i __a = (a); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npshufhw $%i, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a); __a; })

__MLXCC_CONVERT(_mm_cvtepi32_pd, __m128i, __m128d, "cvtdq2pd")
__MLXCC_CONVERT(_mm_cvtepi32_ps, __m128i, __m128, "cvtdq2ps")
__MLXCC_CONVERT(_mm_cvtpd_epi32, __m128d, __m128i, "cvtpd2dq")
__MLXCC_CONVERT(_mm_cvttpd_epi32, __m128d, __m128i, "cvttpd2dq")
__MLXCC_CONVERT(_mm_cvtps_epi32, __m128, __m128i, "cvtps2dq")
__MLXCC_CONVERT(_mm_cvttps_epi32, __m128, __m128i, "cvttps2dq")
#endif
