/* <tmmintrin.h>: SSSE3 (x86-64-v2, not predefined as __SSSE3__): the
   byte shuffle, the absolute values, the alignment shift and the
   multiply-adds as their instructions; the horizontal sums and the
   signs lane by lane. */
#ifndef _MLXCC_TMMINTRIN_H
#define _MLXCC_TMMINTRIN_H
#include <pmmintrin.h>
__MLXCC_BIN(_mm_shuffle_epi8, __m128i, "pshufb")
__MLXCC_UNARY(_mm_abs_epi8, __m128i, "pabsb")
__MLXCC_UNARY(_mm_abs_epi16, __m128i, "pabsw")
__MLXCC_UNARY(_mm_abs_epi32, __m128i, "pabsd")
#define _mm_alignr_epi8(a, b, imm) ({ __m128i __a = (a), __b = (b); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\nmovdqu (%rsi), %xmm1\npalignr $%i, %xmm1, %xmm0\nmovdqu %xmm0, (%rdi)", (imm), &__a, &__b); __a; })
__MLXCC_BIN(_mm_maddubs_epi16, __m128i, "pmaddubsw")
__MLXCC_BIN(_mm_mulhrs_epi16, __m128i, "pmulhrsw")
#define __MLXCC_HORIZONTAL(name, F, n, op, sat) \
    static inline __m128i name(__m128i a, __m128i b) { __m128i r; int i; for (i = 0; i < n / 2; i++) { r.F[i] = sat(a.F[2 * i] op a.F[2 * i + 1]); r.F[i + n / 2] = sat(b.F[2 * i] op b.F[2 * i + 1]); } return r; }
#define __MLXCC_WRAP16(x) ((short)(x))
#define __MLXCC_WRAP32(x) ((int)(x))
__MLXCC_HORIZONTAL(_mm_hadd_epi16, __s, 8, +, __MLXCC_WRAP16)
__MLXCC_HORIZONTAL(_mm_hadd_epi32, __i, 4, +, __MLXCC_WRAP32)
__MLXCC_HORIZONTAL(_mm_hsub_epi16, __s, 8, -, __MLXCC_WRAP16)
__MLXCC_HORIZONTAL(_mm_hsub_epi32, __i, 4, -, __MLXCC_WRAP32)
__MLXCC_HORIZONTAL(_mm_hadds_epi16, __s, 8, +, __mlxcc_sat16)
__MLXCC_HORIZONTAL(_mm_hsubs_epi16, __s, 8, -, __mlxcc_sat16)
#define __MLXCC_SIGN(name, F, n) \
    static inline __m128i name(__m128i a, __m128i b) { int i; for (i = 0; i < n; i++) a.F[i] = b.F[i] < 0 ? -a.F[i] : b.F[i] == 0 ? 0 : a.F[i]; return a; }
__MLXCC_SIGN(_mm_sign_epi8, __b, 16)
__MLXCC_SIGN(_mm_sign_epi16, __s, 8)
__MLXCC_SIGN(_mm_sign_epi32, __i, 4)
#endif
