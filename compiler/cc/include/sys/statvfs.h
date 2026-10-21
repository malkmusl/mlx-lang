#ifndef _MLXCC_SYS_STATVFS_H
#define _MLXCC_SYS_STATVFS_H
#include <sys/types.h>
typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;
struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;
    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;
    fsfilcnt_t f_files;
    fsfilcnt_t f_ffree;
    fsfilcnt_t f_favail;
    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
    int __f_spare[6];
};
#define ST_RDONLY 1
#define ST_NOSUID 2
int statvfs(const char *, struct statvfs *);
int fstatvfs(int, struct statvfs *);
#endif
