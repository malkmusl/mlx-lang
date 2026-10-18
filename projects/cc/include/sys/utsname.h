#ifndef _MLXCC_SYS_UTSNAME_H
#define _MLXCC_SYS_UTSNAME_H
/* As Linux lays it out: six fields of 65 bytes. */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};
int uname(struct utsname *);
#endif
