/* <cpuid.h>: the x86 cpuid instruction, as GCC's header gives it: the
   __cpuid and __cpuid_count macros, __get_cpuid_max, __get_cpuid and
   __get_cpuid_count, and the feature bits of leaves 1 and 7. */
#ifndef __MLXCC_CPUID_H
#define __MLXCC_CPUID_H

/* Leaf 1, ecx. */
#define bit_SSE3 (1 << 0)
#define bit_PCLMUL (1 << 1)
#define bit_SSSE3 (1 << 9)
#define bit_FMA (1 << 12)
#define bit_CMPXCHG16B (1 << 13)
#define bit_SSE4_1 (1 << 19)
#define bit_SSE4_2 (1 << 20)
#define bit_MOVBE (1 << 22)
#define bit_POPCNT (1 << 23)
#define bit_AES (1 << 25)
#define bit_XSAVE (1 << 26)
#define bit_OSXSAVE (1 << 27)
#define bit_AVX (1 << 28)
#define bit_F16C (1 << 29)
#define bit_RDRND (1 << 30)

/* Leaf 1, edx. */
#define bit_CMPXCHG8B (1 << 8)
#define bit_CMOV (1 << 15)
#define bit_MMX (1 << 23)
#define bit_FXSAVE (1 << 24)
#define bit_SSE (1 << 25)
#define bit_SSE2 (1 << 26)

/* Leaf 7 (subleaf 0), ebx. */
#define bit_BMI (1 << 3)
#define bit_AVX2 (1 << 5)
#define bit_BMI2 (1 << 8)
#define bit_ERMS (1 << 9)
#define bit_AVX512F (1 << 16)
#define bit_AVX512DQ (1 << 17)
#define bit_AVX512CD (1 << 28)
#define bit_AVX512BW (1 << 30)
#define bit_AVX512VL (1u << 31)

#define __cpuid(level, a, b, c, d) \
    __asm__ __volatile__("cpuid\n\t" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(level))

#define __cpuid_count(level, count, a, b, c, d) \
    __asm__ __volatile__("cpuid\n\t" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(level), "2"(count))

/* The highest leaf of the range of `ext` (0 or 0x80000000); `sig` the
   vendor's first word when not null. */
static __inline unsigned int __get_cpuid_max(unsigned int ext, unsigned int *sig) {
    unsigned int a, b, c, d;
    __cpuid(ext, a, b, c, d);
    if (sig) *sig = b;
    return a;
}

/* Leaf `leaf` into the four words; 0 when the processor has no such
   leaf. */
static __inline int __get_cpuid(unsigned int leaf, unsigned int *a, unsigned int *b, unsigned int *c, unsigned int *d) {
    unsigned int ext = leaf & 0x80000000;
    unsigned int most = __get_cpuid_max(ext, 0);
    if (most == 0 || most < leaf) return 0;
    __cpuid(leaf, *a, *b, *c, *d);
    return 1;
}

static __inline int __get_cpuid_count(unsigned int leaf, unsigned int subleaf, unsigned int *a, unsigned int *b, unsigned int *c, unsigned int *d) {
    unsigned int ext = leaf & 0x80000000;
    unsigned int most = __get_cpuid_max(ext, 0);
    if (most == 0 || most < leaf) return 0;
    __cpuid_count(leaf, subleaf, *a, *b, *c, *d);
    return 1;
}

#endif
