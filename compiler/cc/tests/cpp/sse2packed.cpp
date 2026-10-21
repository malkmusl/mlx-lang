// SSE2's packed double arithmetic as WebCore's TransformationMatrix
// multiplies 4x4 matrices with it (_mm_load_pd, _mm_set1_pd, _mm_mul_pd,
// _mm_add_pd, _mm_store_pd), and <xmmintrin.h> beside <emmintrin.h>.
#include <cstdio>
#include <xmmintrin.h>
#include <emmintrin.h>
alignas(16) static double a[4] = { 1.5, 2.0, -3.0, 4.25 };
alignas(16) static double out[4];
int main() {
    __m128d x = _mm_load_pd(a);
    __m128d y = _mm_load_pd(a + 2);
    __m128d k = _mm_set1_pd(2.0);
    __m128d sum = _mm_add_pd(_mm_mul_pd(x, k), y);
    _mm_store_pd(out, sum);
    _mm_storeu_pd(out + 2, _mm_div_pd(_mm_sub_pd(y, x), _mm_set_pd(4.0, 2.0)));
    std::printf("%g %g %g %g %g\n", out[0], out[1], out[2], out[3], _mm_cvtsd_f64(_mm_loadu_pd(a + 1)));
    return 0;
}
