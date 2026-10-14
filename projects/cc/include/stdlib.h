#ifndef _MLXCC_STDLIB_H
#define _MLXCC_STDLIB_H
#include <stddef.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647
#define MB_CUR_MAX 1
typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void *reallocarray(void *, size_t, size_t);
void free(void *);
void *aligned_alloc(size_t, size_t);
int posix_memalign(void **, size_t, size_t);
_Noreturn void exit(int);
_Noreturn void _Exit(int);
_Noreturn void abort(void);
/* glibc's libc.so.6 has no atexit of its own (it is __cxa_atexit under
   a static wrapper); mlxlibc has both. */
int __cxa_atexit(void (*)(void *), void *, void *);
static __inline__ int atexit(void (*__function)(void)) { return __cxa_atexit((void (*)(void *))__function, (void *)0, (void *)0); }
int at_quick_exit(void (*)(void));
_Noreturn void quick_exit(int);
int atoi(const char *);
long atol(const char *);
long long atoll(const char *);
double atof(const char *);
long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
double strtod(const char *, char **);
float strtof(const char *, char **);
char *getenv(const char *);
char *secure_getenv(const char *);
int setenv(const char *, const char *, int);
int unsetenv(const char *);
int putenv(char *);
int clearenv(void);
int system(const char *);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
void *bsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *));
int abs(int);
long labs(long);
long long llabs(long long);
div_t div(int, int);
ldiv_t ldiv(long, long);
lldiv_t lldiv(long long, long long);
int rand(void);
void srand(unsigned int);
long random(void);
void srandom(unsigned int);
char *realpath(const char *, char *);
int mkstemp(char *);
char *mkdtemp(char *);
int mblen(const char *, size_t);
int mbtowc(wchar_t *, const char *, size_t);
int wctomb(char *, wchar_t);
size_t mbstowcs(wchar_t *, const char *, size_t);
size_t wcstombs(char *, const wchar_t *, size_t);
#endif
