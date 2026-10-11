#ifndef _MLXCC_SYS_WAIT_H
#define _MLXCC_SYS_WAIT_H
#include <sys/types.h>
#define WNOHANG 1
#define WUNTRACED 2
#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define WTERMSIG(s) ((s) & 0x7f)
#define WIFEXITED(s) (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (((signed char)(((s) & 0x7f) + 1) >> 1) > 0)
#define WIFSTOPPED(s) (((s) & 0xff) == 0x7f)
pid_t wait(int *);
pid_t waitpid(pid_t, int *, int);
#endif
