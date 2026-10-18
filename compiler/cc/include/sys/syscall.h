/* <sys/syscall.h>: the x86-64 Linux system call numbers programs pass
   to syscall() (the common ones). */
#ifndef _MLXCC_SYS_SYSCALL_H
#define _MLXCC_SYS_SYSCALL_H
#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_mmap 9
#define SYS_munmap 11
#define SYS_madvise 28
#define SYS_getpid 39
#define SYS_clone 56
#define SYS_exit 60
#define SYS_kill 62
#define SYS_uname 63
#define SYS_getcwd 79
#define SYS_sysinfo 99
#define SYS_prctl 157
#define SYS_arch_prctl 158
#define SYS_gettid 186
#define SYS_tkill 200
#define SYS_futex 202
#define SYS_sched_getaffinity 204
#define SYS_getdents64 217
#define SYS_clock_gettime 228
#define SYS_exit_group 231
#define SYS_tgkill 234
#define SYS_openat 257
#define SYS_membarrier 324
#define SYS_getrandom 318
#define SYS_memfd_create 319
#define __NR_gettid SYS_gettid
#define __NR_futex SYS_futex
#define __NR_membarrier SYS_membarrier
#define __NR_getrandom SYS_getrandom
#define __NR_memfd_create SYS_memfd_create
#define __NR_tgkill SYS_tgkill
#endif
