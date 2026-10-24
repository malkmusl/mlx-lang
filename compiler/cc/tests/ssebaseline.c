/* x86-64's baseline intrinsics (MMX, SSE, SSE2), as SIMDe's native x86
   path and WebKit's SIMD helpers use them: wrapping and saturating
   arithmetic, comparisons, shifts within and past a lane, packs and
   unpacks, shuffles, the movemasks, conversions (rounding to nearest
   even, the "integer indefinite" value), the MXCSR. */
#include <stdio.h>
#include <emmintrin.h>

static void bytes(const char *name, __m128i v) {
    unsigned char b[16];
    int i;
    _mm_storeu_si128((__m128i *)b, v);
    printf("%s", name);
    for (i = 0; i < 16; i++) printf(" %02x", b[i]);
    printf("\n");
}

static void floats(const char *name, __m128 v) {
    float f[4];
    _mm_storeu_ps(f, v);
    printf("%s %g %g %g %g\n", name, f[0], f[1], f[2], f[3]);
}

int main(void) {
    __m128i a = _mm_setr_epi8(1, -2, 3, 127, -128, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, (char)200);
    __m128i b = _mm_set1_epi8(100);
    bytes("add", _mm_add_epi8(a, b));
    bytes("adds", _mm_adds_epi8(a, b));
    bytes("addsu", _mm_adds_epu8(a, b));
    bytes("subsu", _mm_subs_epu8(a, b));
    bytes("cmpeq", _mm_cmpeq_epi8(a, _mm_setr_epi8(1, 0, 3, 0, -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
    bytes("cmpgt", _mm_cmpgt_epi8(a, _mm_setzero_si128()));
    printf("movemask %04x\n", _mm_movemask_epi8(a));
    bytes("min", _mm_min_epu8(a, b));
    bytes("avg", _mm_avg_epu8(a, b));
    __m128i w = _mm_setr_epi16(1, -1, 300, -300, 32767, -32768, 1000, 7);
    bytes("slli", _mm_slli_epi16(w, 3));
    bytes("srai", _mm_srai_epi16(w, 2));
    bytes("srli", _mm_srli_epi16(w, 17));
    bytes("sra", _mm_sra_epi16(w, _mm_cvtsi32_si128(20)));
    bytes("packs", _mm_packs_epi16(w, w));
    bytes("packus", _mm_packus_epi16(w, w));
    bytes("mulhi", _mm_mulhi_epi16(w, w));
    bytes("madd", _mm_madd_epi16(w, w));
    bytes("unpacklo", _mm_unpacklo_epi8(a, b));
    bytes("unpackhi", _mm_unpackhi_epi16(w, a));
    bytes("shuffle", _mm_shuffle_epi32(a, _MM_SHUFFLE(0, 1, 2, 3)));
    bytes("shufflelo", _mm_shufflelo_epi16(w, _MM_SHUFFLE(0, 0, 3, 1)));
    bytes("bslli", _mm_slli_si128(a, 5));
    bytes("bsrli", _mm_srli_si128(a, 11));
    bytes("mul_epu32", _mm_mul_epu32(_mm_set1_epi32(-1), _mm_set1_epi32(3)));
    bytes("sad", _mm_sad_epu8(a, b));
    printf("extract %d %d\n", _mm_extract_epi16(w, 3), _mm_cvtsi128_si32(_mm_insert_epi16(w, 77, 0)));
    bytes("andnot", _mm_andnot_si128(a, b));
    __m128 f = _mm_setr_ps(1.5f, -2.5f, 3.25f, 0.0f / 0.0f);
    __m128 g = _mm_set1_ps(2.0f);
    floats("addps", _mm_add_ps(_mm_setr_ps(1.5f, -2.5f, 3.25f, 0.5f), g));
    floats("minps", _mm_min_ps(f, g));
    floats("maxps", _mm_max_ps(f, g));
    printf("cmplt %x\n", _mm_movemask_ps(_mm_cmplt_ps(f, g)));
    printf("cmpunord %x\n", _mm_movemask_ps(_mm_cmpunord_ps(f, g)));
    bytes("cvtps", _mm_cvtps_epi32(f));
    bytes("cvttps", _mm_cvttps_epi32(f));
    floats("shufps", _mm_shuffle_ps(f, g, _MM_SHUFFLE(1, 0, 1, 2)));
    floats("unpacklo", _mm_unpacklo_ps(f, g));
    floats("sqrtss", _mm_sqrt_ss(_mm_set_ss(16.0f)));
    floats("cvtepi32", _mm_cvtepi32_ps(_mm_setr_epi32(1, -2, 1 << 24, 7)));
    __m128d d = _mm_setr_pd(2.5, -3.5);
    printf("cvtsd %d %d %lld\n", _mm_cvtsd_si32(d), _mm_cvtsd_si32(_mm_unpackhi_pd(d, d)), _mm_cvttsd_si64(_mm_set_sd(1e30)));
    bytes("cvtpd", _mm_cvtpd_epi32(d));
    printf("movemask_pd %d comilt %d\n", _mm_movemask_pd(d), _mm_comilt_sd(d, _mm_set_sd(3.0)));
    __m64 m = _mm_set_pi16(4, -3, 2, 30000);
    __m64 n = _mm_adds_pi16(m, _mm_set1_pi16(10000));
    printf("mmx %016llx %016llx %d\n", (unsigned long long)_mm_cvtm64_si64(n), (unsigned long long)_mm_cvtm64_si64(_mm_unpacklo_pi16(m, n)), _mm_movemask_pi8(m));
    _mm_empty();
    unsigned int csr = _mm_getcsr();
    printf("rounding %x\n", csr & _MM_ROUND_MASK);
    return 0;
}
