// A constexpr function computing with __int128 (WTF's RapidHash: the
// 128-bit product of two halves, for a static string's hash) is not
// evaluated with 64-bit values: its value is the program's, the high
// half not lost.
#include <cstdio>
#include <cstdint>
#include <utility>
constexpr std::pair<uint64_t, uint64_t> mul(uint64_t a, uint64_t b) { __uint128_t r = static_cast<__uint128_t>(a) * b; return { static_cast<uint64_t>(r), static_cast<uint64_t>(r >> 64) }; }
constexpr uint64_t hi(uint64_t a, uint64_t b) { __uint128_t r = static_cast<__uint128_t>(a) * b; return static_cast<uint64_t>(r >> 64); }
constexpr uint64_t lo(uint64_t a, uint64_t b) { __uint128_t r = static_cast<__uint128_t>(a) * b; return static_cast<uint64_t>(r); }
constexpr uint64_t mix(uint64_t a, uint64_t b) { auto [l, h] = mul(a, b); return l ^ h; }
constexpr uint64_t shifted() { __uint128_t x = 1; x <<= 70; return static_cast<uint64_t>(x >> 64); }
constexpr uint64_t wide() { __uint128_t x = 0xffffffffffffffffull; x += 1; return static_cast<uint64_t>(x >> 64); }
static constexpr auto m = mul(0x1234567890abcdefull, 0xfedcba0987654321ull);
static constexpr uint64_t h = hi(0x1234567890abcdefull, 0xfedcba0987654321ull);
static constexpr uint64_t l = lo(0x1234567890abcdefull, 0xfedcba0987654321ull);
static constexpr uint64_t x = mix(0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull);
static constexpr uint64_t s = shifted();
static constexpr uint64_t w = wide();
int main()
{
    std::printf("%llx %llx | %llx %llx | %llx | %llx %llx\n", (unsigned long long)m.first, (unsigned long long)m.second, (unsigned long long)l, (unsigned long long)h, (unsigned long long)x, (unsigned long long)s, (unsigned long long)w);
    return 0;
}
