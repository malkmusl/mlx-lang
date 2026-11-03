// The assembler's forms for boot code (asm.mlx, top-level assembly):
// 16- and 32-bit code, the system instructions, segment overrides,
// `.org` and `.set`; the bytes printed from the data holding them
// (never run); GNU as encodes them alike.
#include <stdio.h>

asm(".globl boot_code\n"
    "boot_code:\n"
    ".set START, 0x8000\n"
    ".set GDTR, START + 0x60\n"
    ".code16\n"
    "cli\n"
    "xorw %ax, %ax\n"
    "movw %ax, %ds\n"
    "lgdtl GDTR\n"
    "movl %cr0, %eax\n"
    "orb $1, %al\n"
    "movl %eax, %cr0\n"
    "ljmpl $0x08, $0x8040\n"
    ".org 0x20\n"
    ".code32\n"
    "movw $0x10, %ax\n"
    "movw %ax, %ss\n"
    "movl %cr4, %eax\n"
    "orl $0x620, %eax\n"
    "movl %eax, %cr4\n"
    "movl $0x108000, %eax\n"
    "movl %eax, %cr3\n"
    "rdmsr\n"
    "wrmsr\n"
    "movl %ebx, 0x100040\n"
    "ljmpl $0x18, $0x80a0\n"
    ".org 0x80\n"
    ".code64\n"
    "lock xaddl %eax, 0x8130\n"
    "lgdt (%rax)\n"
    "lidt (%rax)\n"
    "lretq\n"
    "ltr %ax\n"
    "movq %rsp, %gs:8\n"
    "movq %gs:0, %rsp\n"
    "pushq %gs:8\n"
    "swapgs\n"
    "movq %gs:0x10, %r14\n"
    "fxsave (%rsp)\n"
    "fxrstor (%rsp)\n"
    "movq %cr2, %rax\n"
    "movq %rdi, %cr3\n"
    "invlpg (%rdi)\n"
    "iretq\n"
    "sti\n"
    "hlt\n"
    "cli\n"
    "outb %al, %dx\n"
    "inb %dx, %al\n"
    "outw %ax, %dx\n"
    "inw %dx, %ax\n"
    "outl %eax, %dx\n"
    "inl %dx, %eax\n"
    "inb $0x60, %al\n"
    "outb %al, $0x80\n"
    "movw %ds, %ax\n"
    "pushfq\n"
    "popfq\n"
    ".globl boot_code_end\n"
    "boot_code_end:\n"
    );

extern const unsigned char boot_code[];
extern const unsigned char boot_code_end[];

int main(void) {
    for (const unsigned char *p = boot_code; p < boot_code_end; p++) printf("%02x", *p);
    printf("\n");
    return 0;
}
