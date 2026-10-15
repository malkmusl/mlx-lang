/* <sys/sysinfo.h>: the system's memory and load (Linux sysinfo), the
   number of processors. */
#ifndef _MLXCC_SYS_SYSINFO_H
#define _MLXCC_SYS_SYSINFO_H
#include <unistd.h>
struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[0];
};
int sysinfo(struct sysinfo *);
static inline int get_nprocs(void) { return (int)sysconf(_SC_NPROCESSORS_ONLN); }
static inline int get_nprocs_conf(void) { return (int)sysconf(_SC_NPROCESSORS_CONF); }
#endif
