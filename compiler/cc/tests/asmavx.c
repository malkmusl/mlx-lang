// The inline assembler's VEX encodings of 128-bit packed values (asm.mlx's
// vexPacked: WebAssembly's SIMD in JavaScriptCore's in-place interpreter)
// and SSE4.1's pmovsx and pmovzx: each instruction's bytes, read back
// from the function holding them (never run), printed one instruction a
// line; GNU as encodes them alike (every form of LLIntAssembly.h's 304).
#include <stdio.h>
#include <string.h>

void code(void) {
    asm volatile(
        ".byte 0xeb,0x06,0x41,0x42,0x43,0x44,0x45,0x46\n"
        "vpaddb %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpaddq %xmm9, %xmm10, %xmm11\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpaddd 16(%rax), %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpshufb (%r9,%r10,4), %xmm12, %xmm1\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpmaxud %xmm8, %xmm1, %xmm15\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vaddps %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vsubpd %xmm13, %xmm14, %xmm0\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vsqrtps %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpabsd %xmm10, %xmm4\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vcvttps2dq %xmm1, %xmm9\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vcvtdq2pd %xmm3, %xmm4\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpmovzxwd %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vptest %xmm1, %xmm1\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vmovddup (%rsp), %xmm1\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vbroadcastss 4(%rdi), %xmm11\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vcmpeqps %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vcmpunordpd %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vcmpgeps %xmm9, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vcmpgtpd %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vcmppd $3, %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vshufps $0x88, %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpblendw $0xaa, %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpinsrb $5, %eax, %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpinsrb $15, (%rsi), %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpinsrw $3, (%r8), %xmm9, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpshufd $0x1b, %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpshuflw $0, %xmm8, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vroundps $3, %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vroundpd $9, %xmm10, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vblendvps %xmm4, %xmm3, %xmm2, %xmm1\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpblendvb %xmm12, %xmm11, %xmm10, %xmm9\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vblendvpd %xmm0, (%rax), %xmm2, %xmm1\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vmovd %eax, %xmm1\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vmovq %rax, %xmm1\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vmovq %r9, %xmm10\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpmovmskb %xmm1, %eax\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpmovmskb %xmm9, %r10d\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpsllw $3, %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpsrad $31, %xmm9, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpsrldq $8, %xmm1, %xmm10\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "vpsrlq %xmm1, %xmm2, %xmm3\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "pmovsxbw (%rax), %xmm0\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "pmovzxdq (%r12), %xmm9\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        "pmovsxwd %xmm1, %xmm2\n"
        ".byte 0x7c,0x7c,0x7c,0x7c\n"
        ".byte 0x46,0x45,0x44,0x43,0x42,0x41");
}

int main(void) {
    const unsigned char *p = (const unsigned char *)code;
    while (memcmp(p, "ABCDEF", 6)) p++;
    p += 6;
    while (memcmp(p, "FEDCBA", 6)) {
        if (!memcmp(p, "||||", 4)) {
            printf("\n");
            p += 4;
            continue;
        }
        printf("%02x", *p++);
    }
    printf("\n");
    return 0;
}
