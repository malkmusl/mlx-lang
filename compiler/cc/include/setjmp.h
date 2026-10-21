#ifndef _MLXCC_SETJMP_H
#define _MLXCC_SETJMP_H
/* The C library's jmp_buf (struct __jmp_buf_tag[1], 200 bytes). */
typedef long jmp_buf[25];
typedef long sigjmp_buf[25];
int setjmp(jmp_buf);
int _setjmp(jmp_buf);
_Noreturn void longjmp(jmp_buf, int);
_Noreturn void _longjmp(jmp_buf, int);
int sigsetjmp(sigjmp_buf, int);
_Noreturn void siglongjmp(sigjmp_buf, int);
#endif
