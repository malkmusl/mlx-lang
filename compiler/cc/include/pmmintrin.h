/* <pmmintrin.h>: SSE3 (x86-64-v2, not predefined as __SSE3__): the
   duplications as their instructions, the horizontal sums lane by
   lane. */
#ifndef _MLXCC_PMMINTRIN_H
#define _MLXCC_PMMINTRIN_H
#include <emmintrin.h>
#define _MM_DENORMALS_ZERO_MASK 0x0040
#define _MM_DENORMALS_ZERO_ON 0x0040
#define _MM_DENORMALS_ZERO_OFF 0x0000
#define _MM_SET_DENORMALS_ZERO_MODE(x) _mm_setcsr((_mm_getcsr() & ~_MM_DENORMALS_ZERO_MASK) | (x))
#define _MM_GET_DENORMALS_ZERO_MODE() (_mm_getcsr() & _MM_DENORMALS_ZERO_MASK)
__MLXCC_UNARY(_mm_movedup_pd, __m128d, "movddup")
static inline __m128d _mm_loaddup_pd(const double *p) { return _mm_load1_pd(p); }
static inline __m128 _mm_moveldup_ps(__m128 a) { a.__f[1] = a.__f[0]; a.__f[3] = a.__f[2]; return a; }
static inline __m128 _mm_movehdup_ps(__m128 a) { a.__f[0] = a.__f[1]; a.__f[2] = a.__f[3]; return a; }
static inline __m128 _mm_hadd_ps(__m128 a, __m128 b) { __m128 r; r.__f[0] = a.__f[0] + a.__f[1]; r.__f[1] = a.__f[2] + a.__f[3]; r.__f[2] = b.__f[0] + b.__f[1]; r.__f[3] = b.__f[2] + b.__f[3]; return r; }
static inline __m128 _mm_hsub_ps(__m128 a, __m128 b) { __m128 r; r.__f[0] = a.__f[0] - a.__f[1]; r.__f[1] = a.__f[2] - a.__f[3]; r.__f[2] = b.__f[0] - b.__f[1]; r.__f[3] = b.__f[2] - b.__f[3]; return r; }
static inline __m128d _mm_hadd_pd(__m128d a, __m128d b) { __m128d r; r.__d[0] = a.__d[0] + a.__d[1]; r.__d[1] = b.__d[0] + b.__d[1]; return r; }
static inline __m128d _mm_hsub_pd(__m128d a, __m128d b) { __m128d r; r.__d[0] = a.__d[0] - a.__d[1]; r.__d[1] = b.__d[0] - b.__d[1]; return r; }
static inline __m128 _mm_addsub_ps(__m128 a, __m128 b) { a.__f[0] -= b.__f[0]; a.__f[1] += b.__f[1]; a.__f[2] -= b.__f[2]; a.__f[3] += b.__f[3]; return a; }
static inline __m128d _mm_addsub_pd(__m128d a, __m128d b) { a.__d[0] -= b.__d[0]; a.__d[1] += b.__d[1]; return a; }
static inline void _mm_monitor(const void *p, unsigned extensions, unsigned hints) { (void)p; (void)extensions; (void)hints; }
static inline void _mm_mwait(unsigned extensions, unsigned hints) { (void)extensions; (void)hints; }
#endif
