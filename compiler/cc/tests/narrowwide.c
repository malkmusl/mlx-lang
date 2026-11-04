// A value narrowed to 32 bits and widened again before a 64-bit
// operation whose result is read whole: the upper half of a product, a
// sum or a shifted value comes from the operands' upper halves, so the
// narrowing stays (the LIR optimizer's dropExtends: WTF's dragonbox read
// `static_cast<uint32_t>(prod) * uint64_t(100)` as the untruncated
// prod, and `prod >> 32` indexed its digit table far past its end).
#include <stdio.h>
#include <stdint.h>
__attribute__((noinline)) static void p2(uint32_t n, char* b) { printf("p2 %u %p\n", n, (void*)b); }
__attribute__((noinline)) static uint64_t step(uint64_t prod) { return (uint32_t)prod * (uint64_t)100; }
__attribute__((noinline)) static uint32_t high(uint64_t prod) { return (uint32_t)(prod >> 32); }
int main(void) {
    uint32_t first_block = 123456789u;
    uint64_t prod = first_block * (uint64_t)1441151882;
    prod >>= 25;
    printf("a %llu\n", (unsigned long long)prod);
    prod = (uint32_t)prod * (uint64_t)100;
    printf("b %llu\n", (unsigned long long)prod);
    p2((uint32_t)(prod >> 32), 0);
    prod = (uint32_t)prod * (uint64_t)100;
    p2((uint32_t)(prod >> 32), 0);
    printf("c %llu\n", (unsigned long long)step(0x1234567890abcdefull));
    printf("d %u\n", high(0x1234567890abcdefull));
    uint64_t q = 0xffffffff12345678ull;
    uint32_t t = (uint32_t)q;
    uint64_t w = t;
    printf("e %llu %u\n", (unsigned long long)w, t);
    uint64_t m = (uint64_t)(uint32_t)q * 3u;
    printf("f %llu\n", (unsigned long long)m);
    int64_t s = -5; int32_t s32 = (int32_t)s; uint64_t us = (uint32_t)s32;
    printf("g %llu\n", (unsigned long long)us);
    uint32_t n = 9; char buffer[16]; char* p = buffer + n * 2;
    printf("h %ld\n", (long)(p - buffer));
    return 0;
}
