/* <linux/memfd.h>: the flags of memfd_create() (the system call is
   syscall(SYS_memfd_create, name, flags), <sys/syscall.h>), the kernel's
   UAPI values. */
#ifndef _MLXCC_LINUX_MEMFD_H
#define _MLXCC_LINUX_MEMFD_H
#define MFD_CLOEXEC 0x0001U
#define MFD_ALLOW_SEALING 0x0002U
#define MFD_HUGETLB 0x0004U
#define MFD_NOEXEC_SEAL 0x0008U
#define MFD_EXEC 0x0010U
/* With MFD_HUGETLB: the page size's log2 in bits 26 to 31. */
#define MFD_HUGE_SHIFT 26
#define MFD_HUGE_MASK 0x3f
#define MFD_HUGE_64KB (16U << MFD_HUGE_SHIFT)
#define MFD_HUGE_512KB (19U << MFD_HUGE_SHIFT)
#define MFD_HUGE_1MB (20U << MFD_HUGE_SHIFT)
#define MFD_HUGE_2MB (21U << MFD_HUGE_SHIFT)
#define MFD_HUGE_8MB (23U << MFD_HUGE_SHIFT)
#define MFD_HUGE_16MB (24U << MFD_HUGE_SHIFT)
#define MFD_HUGE_32MB (25U << MFD_HUGE_SHIFT)
#define MFD_HUGE_256MB (28U << MFD_HUGE_SHIFT)
#define MFD_HUGE_512MB (29U << MFD_HUGE_SHIFT)
#define MFD_HUGE_1GB (30U << MFD_HUGE_SHIFT)
#define MFD_HUGE_2GB (31U << MFD_HUGE_SHIFT)
#define MFD_HUGE_16GB (34U << MFD_HUGE_SHIFT)
#endif
