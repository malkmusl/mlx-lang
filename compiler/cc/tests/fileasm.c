// Top-level assembly (asm.mlx's assembleFile, program.mlx): functions
// defined in asm statements outside functions and called from C, calling
// C back (`call f@plt`), reading C's variables through the GOT and
// RIP-relative, data in the text read RIP-relative, a C function's
// address, alignment with GNU as's no-ops, local and numeric labels, a
// jump too far for rel8 (made rel32) and near ones (rel8), `.size` and
// `.type`; the forms LLIntAssembly.h's interpreter uses. (Its text and
// the probe trampolines' are GNU as's to the byte.)
#include <stdint.h>
#include <stdio.h>

int counter = 5;
int twice(int x) { return x * 2; }

long add3(long, long, long);
long viaC(long);
long readCounter(void);
long farJump(long);
long callThroughTable(long);
long loop(long);
int aligned32(void);
long misc(long);

asm(".text\n"
    ".globl add3\n"
    ".hidden add3\n"
    ".type add3, @function\n"
    "add3:\n"
    "    leaq (%rdi,%rsi), %rax\n"
    "    addq %rdx, %rax\n"
    "    ret\n"
    ".size add3, . - add3\n"
    ".balign 16\n"
    ".globl viaC\n"
    "viaC:\n"
    "    pushq %rbp\n"
    "    movq %rsp, %rbp\n"
    "    call twice@plt\n"
    "    cltq\n"
    "    testq %rax, %rax\n"
    "    jz .Lzero\n"
    "    addq $1000, %rax\n"
    ".Lzero:\n"
    "    popq %rbp\n"
    "    ret\n");

asm(".globl readCounter\n"
    "readCounter:\n"
    "    movq counter@GOTPCREL(%rip), %rcx\n"
    "    movslq (%rcx), %rax\n"
    "    addl counter(%rip), %eax\n"
    "    ret\n"
    ".globl farJump\n"
    "farJump:\n"
    "    movq %rdi, %rax\n"
    "    cmpq $0, %rax\n"
    "    je .Lfar\n"
    "    addq $1, %rax\n"
    "    jmp .Lfar\n"
    "    .skip 200, 0xcc\n"
    ".Lfar:\n"
    "    ret\n"
    ".globl callThroughTable\n"
    "callThroughTable:\n"
    "    pushq %rbp\n"
    "    movq %rsp, %rbp\n"
    "    leaq twice(%rip), %rax\n"
    "    call *%rax\n"
    "    movq .Lconstant(%rip), %rcx\n"
    "    cmpq $77, %rcx\n"
    "    jne 1f\n"
    "    addq $100, %rax\n"
    "1:  popq %rbp\n"
    "    ret\n"
    ".balign 8\n"
    ".Lconstant:\n"
    "    .quad 77\n"
    ".globl loop\n"
    "loop:\n"
    "    xorl %eax, %eax\n"
    "1:  addq %rdi, %rax\n"
    "    decq %rdi\n"
    "    jnz 1b\n"
    "    ret\n"
    ".balign 32\n"
    ".globl aligned32\n"
    "aligned32:\n"
    "    movl $32, %eax\n"
    "    ret\n"
    ".globl misc\n"
    "misc:\n"
    "    pushfq\n"
    "    popfq\n"
    "    movq %rdi, %rax\n"
    "    imull $3, %eax\n"
    "    leal 5(%eax), %ecx\n"
    "    xchgq %rcx, %rax\n"
    "    imulq $-2, %rax, %rdx\n"
    "    negq %rdx\n"
    "    movq %rdx, %rax\n"
    "    ret\n");

asm(".section .note.GNU-stack,\"\",@progbits\n"
    ".previous\n");

int main(void)
{
    printf("add3 %ld\n", add3(1, 2, 3));
    printf("viaC %ld %ld\n", viaC(21), viaC(0));
    printf("readCounter %ld\n", readCounter());
    printf("farJump %ld %ld\n", farJump(0), farJump(41));
    printf("callThroughTable %ld\n", callThroughTable(7));
    printf("loop %ld\n", loop(10));
    printf("aligned32 %d at %d\n", aligned32(), (int)((uintptr_t)aligned32 % 32));
    printf("misc %ld\n", misc(4));
    return 0;
}
