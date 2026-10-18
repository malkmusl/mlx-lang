#ifndef _MLXCC_SYS_MMAN_H
#define _MLXCC_SYS_MMAN_H
#include <sys/types.h>
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_FILE 0
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_FAILED ((void *)-1)
void *mmap(void *, size_t, int, int, int, off_t);
int munmap(void *, size_t);
int mprotect(void *, size_t, int);
#define MREMAP_MAYMOVE 1
#define MREMAP_FIXED 2
void *mremap(void *, size_t, size_t, int, ...);
#define MADV_NORMAL 0
#define MADV_RANDOM 1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#define MADV_FREE 8
#define MADV_REMOVE 9
#define MADV_DONTFORK 10
#define MADV_DOFORK 11
#define MADV_HUGEPAGE 14
#define MADV_NOHUGEPAGE 15
#define MADV_DONTDUMP 16
#define MADV_DODUMP 17
int madvise(void *, size_t, int);
int mincore(void *, size_t, unsigned char *);
#define MS_ASYNC 1
#define MS_INVALIDATE 2
#define MS_SYNC 4
int msync(void *, size_t, int);
int mlock(const void *, size_t);
int munlock(const void *, size_t);
#endif
