// GCC's builtins: the bit builtins (clz, ctz, popcount, bswap, ffs on
// every width, zero included), asm statements of one instruction (a
// compiler fence, pause, nop) and with register operands (cpuid,
// rdtsc, reading the stack pointer), and the builtins that are plain C
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
    long a = 0, b, c, d;
    asm volatile("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d) : : "memory");
    char vendor[13];
    memcpy(vendor, &b, 4);
    memcpy(vendor + 4, &d, 4);
    memcpy(vendor + 8, &c, 4);
    vendor[12] = 0;
    unsigned leaf = 1, eax, ebx, ecx, edx;
    __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(leaf), "2"(0));
    printf("cpuid %d %d %d\n", a > 0, (int)strlen(vendor), eax != 0);
    unsigned lo, hi, lo2, hi2;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    asm volatile("rdtsc" : "=a"(lo2), "=d"(hi2));
    printf("rdtsc %d\n", (((unsigned long long)hi2 << 32) | lo2) >= (((unsigned long long)hi << 32) | lo));
    void *sp = 0;
    __asm__ volatile("movq %%rsp, %0" : "=r"(sp) ::);
    printf("sp %d\n", (char *)&sp - (char *)sp < 4096 && (char *)&sp - (char *)sp > -4096);
    // The overflow builtins: the exact result, kept as the target's type.
    unsigned long ur;
    int ir;
    long lr;
    unsigned uu;
    int o1 = __builtin_mul_overflow(0xffffffffffffffffUL, 2UL, &ur);
    int o2 = __builtin_add_overflow(2147483647, 1, &ir);
    int o3 = __builtin_sub_overflow(0u, 1u, &uu);
    int o4 = __builtin_mul_overflow(-9223372036854775807L - 1, -1L, &lr);
    int o5 = __builtin_add_overflow(-5, 3, &ir);
    printf("overflow %d %lu %d %d %u %d %d %d\n", o1, ur, o2, o3, uu, o4, o5, ir);
    return 0;
}
