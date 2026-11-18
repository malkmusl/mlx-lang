/* <fenv.h>: the floating-point environment as x86-64's C libraries
   number it. mlxlibc's libm keeps the rounding to nearest and raises no
   exception flags (compiler/libc/libm.mlx): fesetround of another mode
   answers nonzero. */
#ifndef _MLXCC_FENV_H
#define _MLXCC_FENV_H
#define FE_INVALID 0x01
#define FE_DIVBYZERO 0x04
#define FE_OVERFLOW 0x08
#define FE_UNDERFLOW 0x10
#define FE_INEXACT 0x20
#define FE_ALL_EXCEPT 0x3d
#define FE_TONEAREST 0
#define FE_DOWNWARD 0x400
#define FE_UPWARD 0x800
#define FE_TOWARDZERO 0xc00
typedef unsigned short fexcept_t;
typedef struct {
    unsigned short __control_word;
    unsigned short __glibc_reserved1;
    unsigned short __status_word;
    unsigned short __glibc_reserved2;
    unsigned short __tags;
    unsigned short __glibc_reserved3;
    unsigned int __eip;
    unsigned short __cs_selector;
    unsigned int __opcode : 11;
    unsigned int __glibc_reserved4 : 5;
    unsigned int __data_offset;
    unsigned short __data_selector;
    unsigned short __glibc_reserved5;
    unsigned int __mxcsr;
} fenv_t;
#define FE_DFL_ENV ((const fenv_t *)-1)
#ifdef __cplusplus
extern "C" {
#endif
int feclearexcept(int);
int fegetexceptflag(fexcept_t *, int);
int feraiseexcept(int);
int fesetexceptflag(const fexcept_t *, int);
int fetestexcept(int);
int fegetround(void);
int fesetround(int);
int fegetenv(fenv_t *);
int feholdexcept(fenv_t *);
int fesetenv(const fenv_t *);
int feupdateenv(const fenv_t *);
#ifdef __cplusplus
}
#endif
#endif
