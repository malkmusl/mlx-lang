// x86-64's baseline SSE2 as SIMDe's native x86 path and WebKit's
// SIMDHelpers.h use it (`#if CPU(X86_64)`): a NEON-style vector type
// whose private union holds an __m128i, converted to it and tested with
// _mm_test_all_zeros, its first and last nonzero lanes found with
// _mm_movemask_epi8 (isNonZero, findFirstNonZeroIndex).
#include <cstdint>
#include <cstdio>
#include <optional>
#include <smmintrin.h>

union uint16x8_private {
    alignas(16) uint16_t values[8];
    __m128i m128i;
};
struct uint16x8 { uint16x8_private p; };

static __m128i toM128i(uint16x8 v) { return v.p.m128i; }

static bool isNonZero(uint16x8 v) {
    auto raw = toM128i(v);
    return !_mm_test_all_zeros(raw, raw);
}

static std::optional<uint8_t> findFirstNonZeroIndex(uint16x8 v) {
    uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi16(toM128i(v), _mm_setzero_si128())) ^ 0xffff;
    if (!mask)
        return std::nullopt;
    return __builtin_ctz(mask) >> 1;
}

static std::optional<uint8_t> findLastNonZeroIndex(uint16x8 v) {
    uint16_t mask = _mm_movemask_epi8(_mm_cmpeq_epi16(toM128i(v), _mm_setzero_si128())) ^ 0xffff;
    if (!mask)
        return std::nullopt;
    return (31 - __builtin_clz(mask)) >> 1;
}

int main() {
    uint16x8 zero { };
    uint16x8 some { };
    some.p.values[2] = 7;
    some.p.values[6] = 0x8000;
    std::printf("%zu %zu\n", sizeof(uint16x8), alignof(__m128i));
    std::printf("%d %d\n", isNonZero(zero), isNonZero(some));
    auto first = findFirstNonZeroIndex(some);
    auto last = findLastNonZeroIndex(some);
    std::printf("%d %d %d\n", first ? *first : -1, last ? *last : -1, findFirstNonZeroIndex(zero) ? 1 : 0);
    uint16x8 sum { };
    _mm_store_si128(&sum.p.m128i, _mm_adds_epu16(toM128i(some), _mm_set1_epi16(-0x7000)));
    for (auto value : sum.p.values)
        std::printf(" %u", value);
    std::printf("\n");
    return 0;
}
