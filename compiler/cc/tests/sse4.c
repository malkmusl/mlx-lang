/* The SSE3, SSSE3, SSE4.1 and SSE4.2 intrinsics (<nmmintrin.h>, x86-64-v2):
   the byte shuffle, blends, tests, the widening conversions, the
   roundings, 32-bit multiplications, the 64-bit comparisons; gcc
   -msse4.2 prints the same. */
#include <stdio.h>
#include <nmmintrin.h>

static void bytes(const char *name, __m128i v) {
    unsigned char b[16];
    int i;
    _mm_storeu_si128((__m128i *)b, v);
    printf("%s", name);
    for (i = 0; i < 16; i++) printf(" %02x", b[i]);
    printf("\n");
}

static void doubles(const char *name, __m128d v) {
    double d[2];
    _mm_storeu_pd(d, v);
    printf("%s %g %g\n", name, d[0], d[1]);
}

static void floats(const char *name, __m128 v) {
    float f[4];
    _mm_storeu_ps(f, v);
    printf("%s %g %g %g %g\n", name, f[0], f[1], f[2], f[3]);
}

int main(void) {
    __m128i a = _mm_setr_epi8(1, -2, 3, 127, -128, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, (char)200);
    __m128i b = _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __m128i mask = _mm_setr_epi8(0, -1, 0, -1, 0, -1, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0);
    bytes("shuffle", _mm_shuffle_epi8(a, b));
    bytes("shuffle_masked", _mm_shuffle_epi8(a, mask));
    bytes("abs8", _mm_abs_epi8(a));
    bytes("abs16", _mm_abs_epi16(a));
    bytes("abs32", _mm_abs_epi32(a));
    bytes("alignr", _mm_alignr_epi8(a, b, 5));
    bytes("maddubs", _mm_maddubs_epi16(a, b));
    bytes("mulhrs", _mm_mulhrs_epi16(a, _mm_set1_epi16(0x4000)));
    bytes("hadd16", _mm_hadd_epi16(a, b));
    bytes("sign8", _mm_sign_epi8(a, mask));
    bytes("blendv", _mm_blendv_epi8(a, b, mask));
    bytes("blend16", _mm_blend_epi16(a, b, 0x5a));
    printf("test %d %d %d %d %d\n", _mm_testz_si128(a, mask), _mm_testz_si128(_mm_setzero_si128(), a), _mm_testc_si128(mask, _mm_setr_epi32(0, -1, 0, 0)), _mm_testnzc_si128(a, mask), _mm_test_all_ones(_mm_set1_epi32(-1)));
    printf("all_zeros %d %d\n", _mm_test_all_zeros(mask, a), _mm_test_all_zeros(_mm_setzero_si128(), a));
    bytes("max8", _mm_max_epi8(a, b));
    bytes("min8", _mm_min_epi8(a, b));
    bytes("maxu16", _mm_max_epu16(a, b));
    bytes("minu16", _mm_min_epu16(a, b));
    bytes("max32", _mm_max_epi32(a, b));
    bytes("min32", _mm_min_epi32(a, b));
    bytes("maxu32", _mm_max_epu32(a, b));
    bytes("minu32", _mm_min_epu32(a, b));
    bytes("mullo32", _mm_mullo_epi32(a, b));
    bytes("mul32", _mm_mul_epi32(a, b));
    bytes("packus32", _mm_packus_epi32(_mm_setr_epi32(-5, 70000, 65535, 3), b));
    bytes("cmpeq64", _mm_cmpeq_epi64(a, _mm_setr_epi32(0x7f03fe01, 0x08070680, 0, 0)));
    bytes("cmpgt64", _mm_cmpgt_epi64(a, b));
    bytes("cvtepi8_16", _mm_cvtepi8_epi16(a));
    bytes("cvtepi8_32", _mm_cvtepi8_epi32(a));
    bytes("cvtepi8_64", _mm_cvtepi8_epi64(a));
    bytes("cvtepi16_32", _mm_cvtepi16_epi32(a));
    bytes("cvtepi32_64", _mm_cvtepi32_epi64(a));
    bytes("cvtepu8_16", _mm_cvtepu8_epi16(a));
    bytes("cvtepu16_64", _mm_cvtepu16_epi64(a));
    bytes("cvtepu32_64", _mm_cvtepu32_epi64(a));
    printf("extract %d %d %lld %d\n", _mm_extract_epi8(a, 15), _mm_extract_epi32(a, 1), _mm_extract_epi64(a, 1), _mm_extract_ps(_mm_set_ps(1.0f, 2.0f, 3.0f, 4.0f), 2));
    bytes("insert", _mm_insert_epi32(_mm_insert_epi8(a, 0x55, 3), 0x11223344, 2));
    bytes("minpos", _mm_minpos_epu16(_mm_setr_epi16(9, 7, 3, 7, 3, 2, 8, 1)));
    __m128d d = _mm_setr_pd(2.5, -3.5);
    doubles("round", _mm_round_pd(d, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
    doubles("floor", _mm_floor_pd(d));
    doubles("ceil", _mm_ceil_pd(d));
    doubles("trunc", _mm_round_pd(d, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC));
    doubles("floor_sd", _mm_floor_sd(d, _mm_set1_pd(7.75)));
    doubles("movedup", _mm_movedup_pd(d));
    doubles("hadd_pd", _mm_hadd_pd(d, _mm_set1_pd(1.25)));
    doubles("blendv_pd", _mm_blendv_pd(d, _mm_set1_pd(9.0), _mm_castsi128_pd(_mm_setr_epi32(0, 0, 0, -1))));
    doubles("dp_pd", _mm_dp_pd(d, _mm_setr_pd(2.0, 4.0), 0x31));
    __m128 f = _mm_setr_ps(1.5f, -2.5f, 3.75f, -4.25f);
    floats("round_ps", _mm_round_ps(f, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
    floats("ceil_ps", _mm_ceil_ps(f));
    floats("blendv_ps", _mm_blendv_ps(f, _mm_set1_ps(9.0f), _mm_castsi128_ps(_mm_setr_epi32(-1, 0, -1, 0))));
    floats("blend_ps", _mm_blend_ps(f, _mm_set1_ps(8.0f), 6));
    floats("dp_ps", _mm_dp_ps(f, _mm_set1_ps(2.0f), 0xf3));
    floats("insert_ps", _mm_insert_ps(f, _mm_set1_ps(7.0f), 0x19));
    floats("hadd_ps", _mm_hadd_ps(f, f));
    floats("moveldup", _mm_moveldup_ps(f));
    printf("popcnt %d %d\n", _mm_popcnt_u32(0xf0f0), (int)_mm_popcnt_u64(0xffffffffffULL));
    return 0;
}
