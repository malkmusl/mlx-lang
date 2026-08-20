global _start
section .text
_start:
  push rbp
  mov rbp, rsp
  sub rsp, 256
.block_0:
.block_1:
  mov rdx, 1
  lea rbx, [rbp - 56]
  mov qword [rbx], rdx
  mov rsi, 2
  lea rdi, [rbp - 96]
  mov qword [rdi], rsi
  mov r8, qword [rbx]
  test r8, r8
  jnz .block_2
  jmp .block_3
.block_2:
  mov r9, qword [rbx]
  mov r10, qword [rdi]
  mov r11, r9
  add r11, r10
  lea r12, [rbp - 176]
  mov qword [r12], r11
  jmp .block_4
.block_3:
  mov r13, qword [rbx]
  mov r14, qword [rdi]
  mov r15, r13
  imul r15, r14
  lea rax, [rbp - 256]
  mov qword [rbp - 128], rax
  mov rax, qword [rbp - 128]
  mov qword [rax], r15
  jmp .block_4
.block_4:
  mov qword [rbp - 136], 0
  lea rax, [rbp - 328]
  mov qword [rbp - 144], rax
  mov rax, qword [rbp - 144]
  mov rcx, qword [rbp - 136]
  mov qword [rax], rcx
  jmp .block_5
.block_5:
  mov rax, qword [rbp - 144]
  mov rcx, qword [rax]
  mov qword [rbp - 152], rcx
  mov rax, qword [rbp - 152]
  test rax, rax
  jnz .block_6
  jmp .block_7
.block_6:
  mov qword [rbp - 160], 5
  lea rax, [rbp - 408]
  mov qword [rbp - 168], rax
  mov rax, qword [rbp - 168]
  mov rcx, qword [rbp - 160]
  mov qword [rax], rcx
  jmp .block_5
.block_7:
  mov rcx, qword [rbx]
  mov qword [rbp - 176], rcx
  mov rcx, qword [rdi]
  mov qword [rbp - 184], rcx
  mov rax, qword [rbp - 176]
  mov qword [rbp - 192], rax
  mov rax, qword [rbp - 184]
  add qword [rbp - 192], rax
  mov rcx, qword [rdi]
  mov qword [rbp - 200], rcx
  mov rax, qword [rbp - 192]
  mov qword [rbp - 208], rax
  mov rax, qword [rbp - 200]
  add qword [rbp - 208], rax
  lea rax, [rbp - 464]
  mov qword [rbp - 216], rax
  mov rax, qword [rbp - 216]
  mov rcx, qword [rbp - 208]
  mov qword [rax], rcx
  mov rax, qword [rbp - 216]
  mov rcx, qword [rax]
  mov qword [rbp - 224], rcx
  mov rax, qword [rbp - 224]
  mov rdi, rax
  mov rax, 60
  syscall
  mov rdi, rax
  mov rax, 60
  syscall
