// GNU inline assembly with operands, assembled by mlx-cc (asm.mlx):
// registers by constraint (a, c, d, D, r, q, g, rm), memory operands,
// ties, named operands with modifiers (%q[x], %k0), local labels, the
// string, SSE and bit instructions; gcc's build prints the same.
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void gcmemcpy(uint64_t *dst, const uint64_t *src, size_t bytes) {
    size_t alignedBytes = (bytes / 64) * 64;
    size_t offset = 0;
    __asm__ volatile(
        ".balign 32\t\n"
        "1:\t\n"
        "cmpq %q[offset], %q[alignedBytes]\t\n"
        "je 2f\t\n"
        "movups (%q[src], %q[offset], 1), %%xmm0\t\n"
        "movups 16(%q[src], %q[offset], 1), %%xmm1\t\n"
        "movups 32(%q[src], %q[offset], 1), %%xmm2\t\n"
        "movups 48(%q[src], %q[offset], 1), %%xmm3\t\n"
        "movups %%xmm0, (%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm1, 16(%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm2, 32(%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm3, 48(%q[dst], %q[offset], 1)\t\n"
        "addq $64, %q[offset]\t\n"
        "jmp 1b\t\n"
        "2:\t\n"
        "cmpq %q[offset], %q[bytes]\t\n"
        "je 3f\t\n"
        "movq (%q[src], %q[offset], 1), %%xmm0\t\n"
        "movq %%xmm0, (%q[dst], %q[offset], 1)\t\n"
        "addq $8, %q[offset]\t\n"
        "jmp 2b\t\n"
        "3:\t\n"
        : [offset] "+r" (offset)
        : [alignedBytes] "r" (alignedBytes), [bytes] "r" (bytes), [dst] "r" (dst), [src] "r" (src)
        : "xmm0", "xmm1", "xmm2", "xmm3", "memory", "cc");
}

static void fill(uint64_t *base, size_t count, uint64_t v) {
    asm volatile("rep stosq\n\t" : "+D"(base), "+c"(count) : "a"(v) : "memory");
}

static uint64_t divide(uint64_t high, uint64_t low, uint64_t divisor, uint64_t *remainder) {
    uint64_t quotient, rem;
    __asm__("divq  %[divisor]" : "=a"(quotient), "=d"(rem) : "d"(high), "a"(low), [divisor] "rm"(divisor));
    *remainder = rem;
    return quotient;
}

struct Regs { uint64_t rbx, r12; };

/* A call of a symbol out of the text (libpas's double-free check in
   pas_segregated_page_inlines.h: btrl, jc over a call). */
int asm_calls;
void asm_note(void) { asm_calls += 1; }

static unsigned clear_bit(unsigned word, unsigned bit) {
    __asm__ volatile("btrl %1, %0\n\tjc 0f\n\tcall asm_note\n\t0:"
                     : "+r"(word) : "r"(bit) : "memory");
    return word;
}

int main(void) {
    uint64_t src[37], dst[37];
    for (int i = 0; i < 37; i++) { src[i] = i * 3 + 1; dst[i] = 0; }
    gcmemcpy(dst, src, 36 * 8);
    printf("%d %llu %llu %llu\n", memcmp(src, dst, 36 * 8), (unsigned long long)dst[35], (unsigned long long)dst[8], (unsigned long long)dst[36]);
    fill(dst, 5, 0x7ff8000000000000ULL);
    printf("%llx %llu\n", (unsigned long long)dst[4], (unsigned long long)dst[5]);
    uint64_t r;
    uint64_t q = divide(3, 7, 10, &r);
    printf("%llu %llu\n", (unsigned long long)q, (unsigned long long)r);
    struct Regs regs;
    asm volatile("movq %%rbx, %0" : "=m"(regs.rbx) : : "memory");
    asm volatile("movq %%r12, %0" : "=m"(regs.r12) : : "memory");
    char c = 0;
    asm volatile("clflush %0" :: "m"(c) : "memory");
    int a = 5, b = 7, out;
    asm("movl %1, %0\n\taddl %2, %0\n\timull $3, %0, %0\n\tshll $2, %0" : "=&r"(out) : "r"(a), "r"(b));
    printf("%d\n", out);
    unsigned char lt;
    asm("cmpl %2, %1\n\tsetl %0" : "=q"(lt) : "r"(a), "r"(b));
    printf("%d\n", lt);
    uint32_t x = 0x11223344;
    asm("bswap %0" : "+r"(x));
    printf("%x\n", x);
    long counter = 10, add = 5;
    asm volatile("lock; xaddq %0, %1" : "+r"(add), "+m"(counter) :: "memory");
    printf("%ld %ld\n", add, counter);
    unsigned char byte = 200;
    unsigned int wide;
    asm("movzbl %1, %0" : "=r"(wide) : "q"(byte));
    printf("%u\n", wide);
    long n = 0;
    asm("xorl %k0, %k0\n1:\n\tincq %0\n\tcmpq $9, %0\n\tjb 1b" : "=r"(n));
    printf("%ld\n", n);
    asm volatile("" : : "g"(n) : "memory");
    asm volatile("nop; pause" ::: "memory");
    unsigned lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    printf("%d\n", hi != 0 || lo != 0);
    uint64_t v = 0x0102030405060708ULL, w;
    asm("movq %1, %%xmm1\n\tmovq %%xmm1, %0" : "=r"(w) : "r"(v) : "xmm1");
    printf("%llx\n", (unsigned long long)w);
    int k = 1 << 20;
    asm("bsrl %1, %0" : "=r"(k) : "r"(k));
    printf("%d\n", k);
    unsigned cleared = clear_bit(6, 2);
    printf("%u %d\n", cleared, asm_calls);
    clear_bit(6, 0);
    asm volatile("call asm_note" ::: "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory");
    printf("%d\n", asm_calls);
    return 0;
}
