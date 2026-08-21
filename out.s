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
  mov r15, 5
  lea rax, [rbp - 32]
  mov qword [rbp - 24], rax
  mov rax, qword [rbp - 24]
  mov qword [rax], r15
  mov rax, 10
  mov qword [rbp - 40], rax
  lea rax, [rbp - 56]
  mov qword [rbp - 48], rax
  mov rax, qword [rbp - 48]
  mov rcx, qword [rbp - 40]
  mov qword [rax], rcx
  lea rax, [rel add]
  mov qword [rbp - 64], rax
  mov rax, qword [rbp - 24]
  mov rcx, qword [rax]
  mov qword [rbp - 72], rcx
  mov rax, qword [rbp - 48]
  mov rcx, qword [rax]
  mov qword [rbp - 80], rcx
  mov rax, qword [rbp - 80]
  mov rsi, rax
  mov rax, qword [rbp - 72]
  mov rdi, rax
  mov rax, qword [rbp - 64]
  call rax
  mov qword [rbp - 88], rax
  lea rax, [rbp - 104]
  mov qword [rbp - 96], rax
  mov rax, qword [rbp - 96]
  mov rcx, qword [rbp - 88]
  mov qword [rax], rcx
  mov rax, qword [rbp - 96]
  mov rcx, qword [rax]
  mov qword [rbp - 112], rcx
  mov rax, qword [rbp - 112]
  mov rsp, rbp
  pop rbp
  ret
  mov rsp, rbp
  pop rbp
  ret
