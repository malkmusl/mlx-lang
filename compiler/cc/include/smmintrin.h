/* <smmintrin.h>: SSE4.1, beyond x86-64's baseline (mlx-cc does not
   predefine __SSE4_1__): SSE2's intrinsics, and _mm_test_all_zeros. */
#ifndef _MLXCC_SMMINTRIN_H
#define _MLXCC_SMMINTRIN_H
#include <emmintrin.h>
static inline int _mm_test_all_zeros(__m128i mask, __m128i a) { return ((mask.__q[0] & a.__q[0]) | (mask.__q[1] & a.__q[1])) == 0; }
#endif
