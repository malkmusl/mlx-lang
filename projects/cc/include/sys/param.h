#ifndef _MLXCC_SYS_PARAM_H
#define _MLXCC_SYS_PARAM_H
#include <sys/types.h>
#include <limits.h>
#define MAXPATHLEN PATH_MAX
#define NOFILE 256
#define NBBY 8
#define MAXHOSTNAMELEN 64
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define powerof2(x) ((((x) - 1) & (x)) == 0)
#endif
