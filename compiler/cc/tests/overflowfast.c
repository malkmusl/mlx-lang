/* __builtin_add_overflow, sub and mul: x, y and *p of one 4- or 8-byte
   integer type are the instruction and its flag (the LIR keeps the
   function); mixed and narrow types are computed exactly; gcc prints
   the same. */
#include <stdio.h>
#include <limits.h>
int main(void) {
    unsigned long u; long s; unsigned u32; int s32; short sh; unsigned char uc; long long ll; unsigned long long ull; int o;
    o = __builtin_add_overflow(~0UL, 2, &u); printf("%d %lu\n", o, u);
    o = __builtin_add_overflow(5UL, 2, &u); printf("%d %lu\n", o, u);
    o = __builtin_add_overflow(0x7fffffffffffffffL, 1, &s); printf("%d %ld\n", o, s);
    o = __builtin_add_overflow(-5L, 1, &s); printf("%d %ld\n", o, s);
    o = __builtin_sub_overflow(-5L, 1, &s); printf("%d %ld\n", o, s);
    o = __builtin_sub_overflow(LONG_MIN, 1L, &s); printf("%d %ld\n", o, s);
    o = __builtin_sub_overflow(3UL, 4UL, &u); printf("%d %lu\n", o, u);
    o = __builtin_mul_overflow(1UL << 40, 1UL << 24, &u); printf("%d %lu\n", o, u);
    o = __builtin_mul_overflow(1UL << 40, 1UL << 23, &u); printf("%d %lu\n", o, u);
    o = __builtin_mul_overflow(-(1L << 40), 1L << 23, &s); printf("%d %ld\n", o, s);
    o = __builtin_mul_overflow(-(1L << 40), 1L << 22, &s); printf("%d %ld\n", o, s);
    o = __builtin_mul_overflow(LONG_MIN, -1L, &s); printf("%d %ld\n", o, s);
    o = __builtin_mul_overflow(1u << 20, 1u << 12, &u32); printf("%d %u\n", o, u32);
    o = __builtin_mul_overflow(1u << 20, 1u << 11, &u32); printf("%d %u\n", o, u32);
    o = __builtin_mul_overflow(-65536, 32768, &s32); printf("%d %d\n", o, s32);
    o = __builtin_mul_overflow(-65536, 32769, &s32); printf("%d %d\n", o, s32);
    o = __builtin_sub_overflow(INT_MIN, 1, &s32); printf("%d %d\n", o, s32);
    o = __builtin_sub_overflow(7, 9, &s32); printf("%d %d\n", o, s32);
    o = __builtin_add_overflow(INT_MAX, 1, &s32); printf("%d %d\n", o, s32);
    o = __builtin_add_overflow(2000000000u, 2000000000u, &u32); printf("%d %u\n", o, u32);
    /* Mixed types and narrow ones: computed exactly. */
    o = __builtin_add_overflow(-1, 1UL, &u); printf("%d %lu\n", o, u);
    o = __builtin_add_overflow(-2, 1UL, &u); printf("%d %lu\n", o, u);
    o = __builtin_add_overflow(30000, 30000, &sh); printf("%d %d\n", o, sh);
    o = __builtin_mul_overflow(16, 16, &uc); printf("%d %d\n", o, uc);
    o = __builtin_add_overflow(1UL << 63, 1UL << 63, &s32); printf("%d %d\n", o, s32);
    o = __builtin_mul_overflow(3000000000u, 2, &u32); printf("%d %u\n", o, u32);
    o = __builtin_mul_overflow(3000000000u, 2, &u); printf("%d %lu\n", o, u);
    o = __builtin_add_overflow(LLONG_MAX, 1LL, &ll); printf("%d %lld\n", o, ll);
    o = __builtin_mul_overflow(ULLONG_MAX, 1ULL, &ull); printf("%d %llu\n", o, ull);
    o = __builtin_smull_overflow(-3L, 5L, &s); printf("%d %ld\n", o, s);
    o = __builtin_uaddll_overflow(ULLONG_MAX, 1ULL, &ull); printf("%d %llu\n", o, ull);
    /* Side effects once. */
    int n = 0; o = __builtin_add_overflow(n++, n++, &s32); printf("%d %d %d\n", o, s32, n);
    return 0;
}
