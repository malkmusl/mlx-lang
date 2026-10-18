#ifndef _MLXCC_SYS_TIME_H
#define _MLXCC_SYS_TIME_H
#include <sys/types.h>
#include <time.h>
struct timeval { time_t tv_sec; suseconds_t tv_usec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };
int gettimeofday(struct timeval *, void *);
int settimeofday(const struct timeval *, const struct timezone *);
int utimes(const char *, const struct timeval[2]);
#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#endif
