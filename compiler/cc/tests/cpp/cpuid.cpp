// <cpuid.h> (as Skia's SkCpu.cpp uses it): the vendor string, the
// feature leaves, xgetbv; the results printed only as facts every x86-64
// processor shares.
#include <cpuid.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    uint32_t abcd[4];
    __cpuid(0, abcd[0], abcd[1], abcd[2], abcd[3]);
    char vendor[13];
    memcpy(vendor, &abcd[1], 4);
    memcpy(vendor + 4, &abcd[3], 4);
    memcpy(vendor + 8, &abcd[2], 4);
    vendor[12] = 0;
    unsigned int a, b, c, d;
    const int got = __get_cpuid(1, &a, &b, &c, &d);
    const bool sse2 = (d & bit_SSE2) != 0;
    __cpuid_count(7, 0, abcd[0], abcd[1], abcd[2], abcd[3]);
    unsigned int most = __get_cpuid_max(0, nullptr);
    uint32_t eax, edx;
    __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    printf("%zu %d %d %d %d\n", strlen(vendor), got, sse2, most >= 1, (eax & 3) == 3);
    return argc;
}
