/* <sys/prctl.h>: prctl() and its options (Linux). */
#ifndef _MLXCC_SYS_PRCTL_H
#define _MLXCC_SYS_PRCTL_H
#define PR_SET_PDEATHSIG 1
#define PR_GET_PDEATHSIG 2
#define PR_GET_DUMPABLE 3
#define PR_SET_DUMPABLE 4
#define PR_SET_NAME 15
#define PR_GET_NAME 16
#define PR_SET_TIMERSLACK 29
#define PR_SET_NO_NEW_PRIVS 38
#define PR_GET_NO_NEW_PRIVS 39
#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0
int prctl(int option, ...);
#endif
