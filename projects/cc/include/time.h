#ifndef _MLXCC_TIME_H
#define _MLXCC_TIME_H
#include <stddef.h>
typedef long time_t;
typedef long clock_t;
typedef int clockid_t;
struct timespec { time_t tv_sec; long tv_nsec; };
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};
#define CLOCKS_PER_SEC 1000000L
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define TIME_UTC 1
time_t time(time_t *);
clock_t clock(void);
double difftime(time_t, time_t);
time_t mktime(struct tm *);
time_t timegm(struct tm *);
struct tm *localtime(const time_t *);
struct tm *gmtime(const time_t *);
struct tm *localtime_r(const time_t *, struct tm *);
struct tm *gmtime_r(const time_t *, struct tm *);
char *asctime(const struct tm *);
char *ctime(const time_t *);
size_t strftime(char *, size_t, const char *, const struct tm *);
int clock_gettime(clockid_t, struct timespec *);
int clock_getres(clockid_t, struct timespec *);
int nanosleep(const struct timespec *, struct timespec *);
int timespec_get(struct timespec *, int);
void tzset(void);
#endif
