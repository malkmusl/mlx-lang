/* <nmmintrin.h>: SSE4.2 (x86-64-v2, not predefined as __SSE4_2__): the
   64-bit comparison and the population counts; SSE4.1 comes with it. */
#ifndef _MLXCC_NMMINTRIN_H
#define _MLXCC_NMMINTRIN_H
#include <smmintrin.h>
__MLXCC_BIN(_mm_cmpgt_epi64, __m128i, "pcmpgtq")
#define _mm_popcnt_u32(x) __builtin_popcount(x)
#define _mm_popcnt_u64(x) __builtin_popcountll(x)
#endif
