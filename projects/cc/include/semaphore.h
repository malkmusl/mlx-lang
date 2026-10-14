#ifndef _MLXCC_SEMAPHORE_H
#define _MLXCC_SEMAPHORE_H
#include <sys/types.h>
#include <time.h>
typedef union { char __size[32]; long __align; } sem_t;
#define SEM_FAILED ((sem_t *)0)
#define SEM_VALUE_MAX 2147483647
int sem_init(sem_t *, int, unsigned int);
int sem_destroy(sem_t *);
sem_t *sem_open(const char *, int, ...);
int sem_close(sem_t *);
int sem_unlink(const char *);
int sem_wait(sem_t *);
int sem_trywait(sem_t *);
int sem_timedwait(sem_t *, const struct timespec *);
int sem_post(sem_t *);
int sem_getvalue(sem_t *, int *);
#endif
