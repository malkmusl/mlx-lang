/* <smmintrin.h>: SSE4.1 (x86-64-v2, not predefined as __SSE4_1__): the
   blends, the tests, the minimums and maximums, the widening
   conversions, the roundings and the 32-bit multiplications as their
   instructions; the extractions, insertions and the dot products lane
   by lane. SSSE3 and SSE3 come with it. */
#ifndef _MLXCC_SMMINTRIN_H
#define _MLXCC_SMMINTRIN_H
#include <tmmintrin.h>
#define _MM_FROUND_TO_NEAREST_INT 0x00
#define _MM_FROUND_TO_NEG_INF 0x01
#define _MM_FROUND_TO_POS_INF 0x02
#define _MM_FROUND_TO_ZERO 0x03
#define _MM_FROUND_CUR_DIRECTION 0x04
#define _MM_FROUND_RAISE_EXC 0x00
#define _MM_FROUND_NO_EXC 0x08
#define _MM_FROUND_NINT (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_FLOOR (_MM_FROUND_TO_NEG_INF | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_CEIL (_MM_FROUND_TO_POS_INF | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_TRUNC (_MM_FROUND_TO_ZERO | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_RINT (_MM_FROUND_CUR_DIRECTION | _MM_FROUND_RAISE_EXC)
#define _MM_FROUND_NEARBYINT (_MM_FROUND_CUR_DIRECTION | _MM_FROUND_NO_EXC)

/* The blends: b's lanes where the mask's are set (the top bit of each
   byte, or of each lane for the floating-point ones). */
static inline __m128i _mm_blendv_epi8(__m128i a, __m128i b, __m128i mask) { __builtin_mlxcc_asm("movdqu (%rdx), %xmm0\nmovdqu (%rdi), %xmm1\nmovdqu (%rsi), %xmm2\npblendvb %xmm0, %xmm2, %xmm1\nmovdqu %xmm1, (%rdi)", &a, &b, &mask); return a; }
static inline __m128 _mm_blendv_ps(__m128 a, __m128 b, __m128 mask) { __builtin_mlxcc_asm("movups (%rdx), %xmm0\nmovups (%rdi), %xmm1\nmovups (%rsi), %xmm2\nblendvps %xmm0, %xmm2, %xmm1\nmovups %xmm1, (%rdi)", &a, &b, &mask); return a; }
static inline __m128d _mm_blendv_pd(__m128d a, __m128d b, __m128d mask) { __builtin_mlxcc_asm("movupd (%rdx), %xmm0\nmovupd (%rdi), %xmm1\nmovupd (%rsi), %xmm2\nblendvpd %xmm0, %xmm2, %xmm1\nmovupd %xmm1, (%rdi)", &a, &b, &mask); return a; }
#define _mm_blend_epi16(a, b, imm) ({ __m128i __a = (a), __b = (b); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\nmovdqu (%rsi), %xmm1\npblendw $%i, %xmm1, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a, &__b); __a; })
static inline __m128 _mm_blend_ps(__m128 a, __m128 b, int imm) { int i; for (i = 0; i < 4; i++) if (imm & (1 << i)) a.__f[i] = b.__f[i]; return a; }
static inline __m128d _mm_blend_pd(__m128d a, __m128d b, int imm) { if (imm & 1) a.__d[0] = b.__d[0]; if (imm & 2) a.__d[1] = b.__d[1]; return a; }

/* ptest: ZF when a & mask is zero, CF when ~a & mask is. */
static inline int _mm_testz_si128(__m128i a, __m128i mask) { return (int)__builtin_mlxcc_asm("movdqu (%rdi), %xmm0\nmovdqu (%rsi), %xmm1\nptest %xmm1, %xmm0\nsete %al\nmovzbl %al, %eax", &a, &mask); }
static inline int _mm_testc_si128(__m128i a, __m128i mask) { return (int)__builtin_mlxcc_asm("movdqu (%rdi), %xmm0\nmovdqu (%rsi), %xmm1\nptest %xmm1, %xmm0\nsetb %al\nmovzbl %al, %eax", &a, &mask); }
static inline int _mm_testnzc_si128(__m128i a, __m128i mask) { return (int)__builtin_mlxcc_asm("movdqu (%rdi), %xmm0\nmovdqu (%rsi), %xmm1\nptest %xmm1, %xmm0\nseta %al\nmovzbl %al, %eax", &a, &mask); }
static inline int _mm_test_all_zeros(__m128i mask, __m128i a) { return _mm_testz_si128(a, mask); }
static inline int _mm_test_all_ones(__m128i a) { return _mm_testc_si128(a, _mm_cmpeq_epi32(a, a)); }
static inline int _mm_test_mix_ones_zeros(__m128i a, __m128i mask) { return _mm_testnzc_si128(a, mask); }

__MLXCC_BIN(_mm_max_epi8, __m128i, "pmaxsb")
__MLXCC_BIN(_mm_max_epu16, __m128i, "pmaxuw")
__MLXCC_BIN(_mm_max_epi32, __m128i, "pmaxsd")
__MLXCC_BIN(_mm_max_epu32, __m128i, "pmaxud")
__MLXCC_BIN(_mm_min_epi8, __m128i, "pminsb")
__MLXCC_BIN(_mm_min_epu16, __m128i, "pminuw")
__MLXCC_BIN(_mm_min_epi32, __m128i, "pminsd")
__MLXCC_BIN(_mm_min_epu32, __m128i, "pminud")
__MLXCC_BIN(_mm_mullo_epi32, __m128i, "pmulld")
__MLXCC_BIN(_mm_mul_epi32, __m128i, "pmuldq")
__MLXCC_BIN(_mm_packus_epi32, __m128i, "packusdw")
__MLXCC_BIN(_mm_cmpeq_epi64, __m128i, "pcmpeqq")
__MLXCC_UNARY(_mm_cvtepi8_epi16, __m128i, "pmovsxbw")
__MLXCC_UNARY(_mm_cvtepi8_epi32, __m128i, "pmovsxbd")
__MLXCC_UNARY(_mm_cvtepi8_epi64, __m128i, "pmovsxbq")
__MLXCC_UNARY(_mm_cvtepi16_epi32, __m128i, "pmovsxwd")
__MLXCC_UNARY(_mm_cvtepi16_epi64, __m128i, "pmovsxwq")
__MLXCC_UNARY(_mm_cvtepi32_epi64, __m128i, "pmovsxdq")
__MLXCC_UNARY(_mm_cvtepu8_epi16, __m128i, "pmovzxbw")
__MLXCC_UNARY(_mm_cvtepu8_epi32, __m128i, "pmovzxbd")
__MLXCC_UNARY(_mm_cvtepu8_epi64, __m128i, "pmovzxbq")
__MLXCC_UNARY(_mm_cvtepu16_epi32, __m128i, "pmovzxwd")
__MLXCC_UNARY(_mm_cvtepu16_epi64, __m128i, "pmovzxwq")
__MLXCC_UNARY(_mm_cvtepu32_epi64, __m128i, "pmovzxdq")

/* The roundings, by the immediate's mode (_MM_FROUND_*). */
#define _mm_round_pd(a, imm) ({ __m128d __a = (a); __builtin_mlxcc_asmi("movupd (%rdi), %xmm0\nroundpd $%i, %xmm0, %xmm0\nmovupd %xmm0, (%rdi)", (imm), &__a); __a; })
#define _mm_round_ps(a, imm) ({ __m128 __a = (a); __builtin_mlxcc_asmi("movups (%rdi), %xmm0\nroundps $%i, %xmm0, %xmm0\nmovups %xmm0, (%rdi)", (imm), &__a); __a; })
#define _mm_round_sd(a, b, imm) ({ __m128d __a = (a), __b = (b); __builtin_mlxcc_asmi("movupd (%rdi), %xmm0\nmovupd (%rsi), %xmm1\nroundsd $%i, %xmm1, %xmm0\nmovupd %xmm0, (%rdi)", (imm), &__a, &__b); __a; })
#define _mm_round_ss(a, b, imm) ({ __m128 __a = (a), __b = (b); __builtin_mlxcc_asmi("movups (%rdi), %xmm0\nmovups (%rsi), %xmm1\nroundss $%i, %xmm1, %xmm0\nmovups %xmm0, (%rdi)", (imm), &__a, &__b); __a; })
#define _mm_floor_pd(a) _mm_round_pd((a), _MM_FROUND_FLOOR)
#define _mm_floor_ps(a) _mm_round_ps((a), _MM_FROUND_FLOOR)
#define _mm_floor_sd(a, b) _mm_round_sd((a), (b), _MM_FROUND_FLOOR)
#define _mm_floor_ss(a, b) _mm_round_ss((a), (b), _MM_FROUND_FLOOR)
#define _mm_ceil_pd(a) _mm_round_pd((a), _MM_FROUND_CEIL)
#define _mm_ceil_ps(a) _mm_round_ps((a), _MM_FROUND_CEIL)
#define _mm_ceil_sd(a, b) _mm_round_sd((a), (b), _MM_FROUND_CEIL)
#define _mm_ceil_ss(a, b) _mm_round_ss((a), (b), _MM_FROUND_CEIL)

/* Lanes out and in. */
static inline int _mm_extract_epi8(__m128i a, int imm) { return a.__ub[imm & 15]; }
static inline int _mm_extract_epi32(__m128i a, int imm) { return a.__i[imm & 3]; }
static inline long long _mm_extract_epi64(__m128i a, int imm) { return a.__q[imm & 1]; }
static inline int _mm_extract_ps(__m128 a, int imm) { return a.__i[imm & 3]; }
static inline __m128i _mm_insert_epi8(__m128i a, int d, int imm) { a.__b[imm & 15] = (signed char)d; return a; }
static inline __m128i _mm_insert_epi32(__m128i a, int d, int imm) { a.__i[imm & 3] = d; return a; }
static inline __m128i _mm_insert_epi64(__m128i a, long long d, int imm) { a.__q[imm & 1] = d; return a; }
static inline __m128 _mm_insert_ps(__m128 a, __m128 b, int imm) {
    int i;
    a.__f[(imm >> 4) & 3] = b.__f[(imm >> 6) & 3];
    for (i = 0; i < 4; i++) if (imm & (1 << i)) a.__f[i] = 0.0f;
    return a;
}
static inline __m128 _mm_dp_ps(__m128 a, __m128 b, int imm) {
    float s = 0.0f; int i; __m128 r;
    for (i = 0; i < 4; i++) if (imm & (16 << i)) s += a.__f[i] * b.__f[i];
    for (i = 0; i < 4; i++) r.__f[i] = imm & (1 << i) ? s : 0.0f;
    return r;
}
static inline __m128d _mm_dp_pd(__m128d a, __m128d b, int imm) {
    double s = 0.0; __m128d r;
    if (imm & 16) s += a.__d[0] * b.__d[0];
    if (imm & 32) s += a.__d[1] * b.__d[1];
    r.__d[0] = imm & 1 ? s : 0.0;
    r.__d[1] = imm & 2 ? s : 0.0;
    return r;
}
static inline __m128i _mm_minpos_epu16(__m128i a) {
    int i, at = 0; __m128i r = _mm_setzero_si128();
    for (i = 1; i < 8; i++) if (a.__us[i] < a.__us[at]) at = i;
    r.__us[0] = a.__us[at]; r.__us[1] = (unsigned short)at;
    return r;
}
static inline __m128i _mm_stream_load_si128(const __m128i *p) { return _mm_loadu_si128(p); }
#endif
