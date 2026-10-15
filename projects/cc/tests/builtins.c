// GCC's builtins: the bit builtins (clz, ctz, popcount, bswap, ffs on
// every width, zero included), asm statements of one instruction (a
// compiler fence, pause, nop), and the builtins that are plain C
// (memcpy, expect, isnan, inf, object_size).
#include <stdio.h>
#include <string.h>
#include <math.h>
int main(void) {
    unsigned v = 0x00f0f000u;
    unsigned long l = 0x0000000800000000ul;
    printf("clz %d %d %d\n", __builtin_clz(v), __builtin_clzl(l), __builtin_clzll(1ull));
    printf("ctz %d %d %d\n", __builtin_ctz(v), __builtin_ctzl(l), __builtin_ctzll(0x8000000000000000ull));
    printf("popcount %d %d %d\n", __builtin_popcount(v), __builtin_popcountl(l), __builtin_popcountll(0xffffffffffffffffull));
    printf("bswap %x %x %llx\n", (unsigned)__builtin_bswap16(0x1234), __builtin_bswap32(0x12345678u), __builtin_bswap64(0x0102030405060708ull));
    printf("ffs %d %d %d %d\n", __builtin_ffs(0), __builtin_ffs(8), __builtin_ffsl(1l << 40), __builtin_ffsll(0));
    printf("zero %d %d %d %d\n", __builtin_popcount(0), __builtin_clz(1), __builtin_ctz(1), __builtin_clzl(1ul));
    printf("signed %d %d\n", __builtin_clz(-1), __builtin_popcount(-1));
    char buf[8];
    __builtin_memcpy(buf, "abc", 4);
    __builtin_memset(buf + 3, 'x', 2);
    buf[5] = 0;
    printf("mem %s %d %d\n", buf, (int)__builtin_strlen(buf), __builtin_memcmp(buf, "abcxx", 5));
    double nan = __builtin_nan("");
    double inf = __builtin_inf();
    printf("fp %d %d %d %d %d\n", __builtin_isnan(nan), __builtin_isinf(inf), __builtin_isfinite(2.5), __builtin_isinf(-inf), __builtin_isnan(inf));
    printf("expect %d %d %d\n", __builtin_expect(3, 1), __builtin_expect_with_probability(4, 1, 0.9), __builtin_constant_p(buf[0]));
    __asm__ volatile("" ::: "memory");
    asm("pause");
    __asm__("nop");
    __asm__ __volatile__("mfence");
    printf("asm ok\n");
    return 0;
}
