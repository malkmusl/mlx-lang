#ifndef _MLXCC_UNISTD_H
#define _MLXCC_UNISTD_H
#include <sys/types.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_CONF 83
#define _SC_NPROCESSORS_ONLN 84
#define _SC_CLK_TCK 2
#define _SC_OPEN_MAX 4
extern char **environ;
extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int, char *const[], const char *);
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);
ssize_t pread(int, void *, size_t, off_t);
ssize_t pwrite(int, const void *, size_t, off_t);
int close(int);
off_t lseek(int, off_t, int);
int pipe(int[2]);
int dup(int);
int dup2(int, int);
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int setuid(uid_t);
int setgid(gid_t);
pid_t setsid(void);
int execv(const char *, char *const[]);
int execvp(const char *, char *const[]);
int execve(const char *, char *const[], char *const[]);
int execl(const char *, const char *, ...);
int execlp(const char *, const char *, ...);
_Noreturn void _exit(int);
unsigned int sleep(unsigned int);
int usleep(useconds_t);
unsigned int alarm(unsigned int);
int pause(void);
int access(const char *, int);
int unlink(const char *);
int rmdir(const char *);
int link(const char *, const char *);
int symlink(const char *, const char *);
ssize_t readlink(const char *, char *, size_t);
int chdir(const char *);
int fchdir(int);
char *getcwd(char *, size_t);
int chown(const char *, uid_t, gid_t);
int fchown(int, uid_t, gid_t);
int lchown(const char *, uid_t, gid_t);
int fdatasync(int);
int truncate(const char *, off_t);
int ftruncate(int, off_t);
int fsync(int);
int isatty(int);
char *ttyname(int);
long sysconf(int);
// A system call by its number (<sys/syscall.h>).
long syscall(long, ...);
int gethostname(char *, size_t);
int getpagesize(void);
#endif
