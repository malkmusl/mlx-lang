/* __builtin_mlxcc_asm(text, a, b, c): the text with a, b and c in rdi,
   rsi and rdx, the result in rax; __builtin_mlxcc_asmi with a constant
   for `%i`; inlined or not, on either back end alike. */
#include <stdio.h>

typedef union __attribute__((__aligned__(16))) { unsigned char b[16]; long long q[2]; } V;

static inline V add8(V a, V b) { __builtin_mlxcc_asm("movdqu (%rdi), %xmm0\nmovdqu (%rsi), %xmm1\npaddb %xmm1, %xmm0\nmovdqu %xmm0, (%rdi)", &a, &b); return a; }
static inline int mask(V a) { return (int)__builtin_mlxcc_asm("movdqu (%rdi), %xmm0\npmovmskb %xmm0, %eax", &a); }
#define shuffle(a, i) ({ V __a = (a); __builtin_mlxcc_asmi("movdqu (%rdi), %xmm0\npshufd $%i, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", (i), &__a); __a; })
static inline V broadcast(int v) { V r; __builtin_mlxcc_asm("movd %esi, %xmm0\npunpcklbw %xmm0, %xmm0\npunpcklwd %xmm0, %xmm0\npshufd $0, %xmm0, %xmm0\nmovdqu %xmm0, (%rdi)", &r, v); return r; }
static inline V sums(V a, V b) { __builtin_mlxcc_asm("movdqu (%rdi), %xmm0\nmovdqu (%rsi), %xmm1\npsadbw %xmm1, %xmm0\nmovdqu %xmm0, (%rdi)", &a, &b); return a; }
static inline V blend(V a, V b, V m) { __builtin_mlxcc_asm("movdqu (%rdx), %xmm0\nmovdqu (%rdi), %xmm1\nmovdqu (%rsi), %xmm2\npblendvb %xmm0, %xmm2, %xmm1\nmovdqu %xmm1, (%rdi)", &a, &b, &m); return a; }
__attribute__((noinline)) static V add8_called(V a, V b) { return add8(a, b); }
/* No operand: the text alone; a call in the text finds the stack aligned. */
static int answer(void) { return (int)__builtin_mlxcc_asm("movl $42, %eax"); }
static long doubled(long x) { return __builtin_mlxcc_asm("leaq (%rdi,%rdi), %rax", x); }

static void show(const char *name, V v) {
    int i;
    printf("%s", name);
    for (i = 0; i < 16; i++) printf(" %02x", v.b[i]);
    printf("\n");
}

int main(void) {
    V a, b, c;
    int i;
    for (i = 0; i < 16; i++) {
        a.b[i] = (unsigned char)i;
        b.b[i] = (unsigned char)(0x80 + i * 7);
    }
    show("add", add8(a, b));
    show("add_called", add8_called(a, b));
    printf("mask %04x %04x\n", mask(a), mask(b));
    show("shuffle", shuffle(a, 0x1b));
    show("broadcast", broadcast(0x42));
    show("sums", sums(a, b));
    c = broadcast(0x80);
    for (i = 0; i < 16; i += 2) c.b[i] = 0;
    show("blend", blend(a, b, c));
    double d = 2.5;
    printf("cvt %lld %d %ld\n", (long long)__builtin_mlxcc_asm("cvttsd2si (%rdi), %rax", &d), answer(), doubled(21));
    return 0;
}
