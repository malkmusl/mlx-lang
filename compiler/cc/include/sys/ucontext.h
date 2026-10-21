/* <sys/ucontext.h>: the machine context a signal handler is given (its
   third argument) on x86-64 Linux, laid out as the kernel writes it:
   the general registers by REG_ index, the floating-point state. */
#ifndef _MLXCC_SYS_UCONTEXT_H
#define _MLXCC_SYS_UCONTEXT_H
#include <signal.h>

typedef long long greg_t;
#define NGREG 23
typedef greg_t gregset_t[NGREG];

enum {
    REG_R8 = 0, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15,
    REG_RDI, REG_RSI, REG_RBP, REG_RBX, REG_RDX, REG_RAX, REG_RCX, REG_RSP,
    REG_RIP, REG_EFL, REG_CSGSFS, REG_ERR, REG_TRAPNO, REG_OLDMASK, REG_CR2
};
#define REG_R8 REG_R8
#define REG_RIP REG_RIP
#define REG_RSP REG_RSP
#define REG_RBP REG_RBP

struct _libc_fpxreg { unsigned short significand[4]; unsigned short exponent; unsigned short __glibc_reserved1[3]; };
struct _libc_xmmreg { unsigned int element[4]; };
struct _libc_fpstate {
    unsigned short cwd;
    unsigned short swd;
    unsigned short ftw;
    unsigned short fop;
    unsigned long rip;
    unsigned long rdp;
    unsigned int mxcsr;
    unsigned int mxcr_mask;
    struct _libc_fpxreg _st[8];
    struct _libc_xmmreg _xmm[16];
    unsigned int __glibc_reserved1[24];
};
typedef struct _libc_fpstate *fpregset_t;

typedef struct {
    gregset_t gregs;
    fpregset_t fpregs;
    unsigned long long __reserved1[8];
} mcontext_t;

typedef struct ucontext_t {
    unsigned long uc_flags;
    struct ucontext_t *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
    struct _libc_fpstate __fpregs_mem;
    unsigned long long __ssp[4];
} ucontext_t;
#endif
