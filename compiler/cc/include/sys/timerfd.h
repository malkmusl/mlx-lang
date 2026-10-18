#ifndef _MLXCC_SYS_TIMERFD_H
#define _MLXCC_SYS_TIMERFD_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _MLXCC_ITIMERSPEC
#define _MLXCC_ITIMERSPEC
struct itimerspec { struct timespec it_interval; struct timespec it_value; };
#endif
#define TFD_NONBLOCK 04000
#define TFD_CLOEXEC 02000000
#define TFD_TIMER_ABSTIME 1
#define TFD_TIMER_CANCEL_ON_SET 2
int timerfd_create(int, int);
int timerfd_settime(int, int, const struct itimerspec *, struct itimerspec *);
int timerfd_gettime(int, struct itimerspec *);
#ifdef __cplusplus
}
#endif
#endif
