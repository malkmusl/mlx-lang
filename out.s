global _start
section .text
_start:
  call main
  mov rdi, rax
  mov rax, 60
  syscall
.block_0:
.block_1:
global add
add:
  push rbp
  mov rbp, rsp
  sub rsp, 256
  mov rdx, rdi
  lea rbx, [rbp - 8]
  mov qword [rbx], rdx
  mov r10, rsi
  lea r11, [rbp - 16]
  mov qword [r11], r10
  mov r12, qword [rbx]
  mov r13, qword [r11]
  mov r14, r12
  add r14, r13
  mov rax, r14
  mov rsp, rbp
  pop rbp
  ret
  mov rsp, rbp
  pop rbp
  ret
.block_2:
global main
main:
  push rbp
  mov rbp, rsp
  sub rsp, 256
  lea r15, [rel add]
  mov rax, 10
  mov qword [rbp - 24], rax
  mov rax, 5
  mov qword [rbp - 32], rax
  mov rax, qword [rbp - 32]
  mov rsi, rax
  mov rax, qword [rbp - 24]
  mov rdi, rax
  call r15
  mov qword [rbp - 40], rax
  lea rax, [rbp - 56]
  mov qword [rbp - 48], rax
  mov rax, qword [rbp - 48]
  mov rcx, qword [rbp - 40]
  mov qword [rax], rcx
  mov rax, qword [rbp - 48]
  mov rcx, qword [rax]
  mov qword [rbp - 64], rcx
  mov rax, qword [rbp - 64]
  mov rsp, rbp
  pop rbp
  ret
  mov rsp, rbp
  pop rbp
  ret
