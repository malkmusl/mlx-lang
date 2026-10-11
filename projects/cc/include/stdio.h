#ifndef _MLXCC_STDIO_H
#define _MLXCC_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct _IO_FILE FILE;
typedef long off_t;
typedef long ssize_t;
typedef struct { long __position; long __state; } fpos_t;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
#define stdin stdin
#define stdout stdout
#define stderr stderr
#define EOF (-1)
#define BUFSIZ 8192
#define FILENAME_MAX 4096
#define FOPEN_MAX 16
#define L_tmpnam 20
#define TMP_MAX 238328
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
int dprintf(int, const char *, ...);
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vprintf(const char *, va_list);
int vfprintf(FILE *, const char *, va_list);
int vdprintf(int, const char *, va_list);
int vsprintf(char *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);
int asprintf(char **, const char *, ...);
int vasprintf(char **, const char *, va_list);
int scanf(const char *, ...);
int fscanf(FILE *, const char *, ...);
int sscanf(const char *, const char *, ...);
int vsscanf(const char *, const char *, va_list);
int puts(const char *);
int fputs(const char *, FILE *);
int putchar(int);
int fputc(int, FILE *);
int putc(int, FILE *);
int getchar(void);
int fgetc(FILE *);
int getc(FILE *);
int ungetc(int, FILE *);
char *fgets(char *, int, FILE *);
ssize_t getline(char **, size_t *, FILE *);
ssize_t getdelim(char **, size_t *, int, FILE *);
FILE *fopen(const char *, const char *);
FILE *freopen(const char *, const char *, FILE *);
FILE *fdopen(int, const char *);
FILE *fmemopen(void *, size_t, const char *);
FILE *tmpfile(void);
FILE *popen(const char *, const char *);
int pclose(FILE *);
int fclose(FILE *);
int fflush(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
int fseeko(FILE *, off_t, int);
long ftell(FILE *);
off_t ftello(FILE *);
void rewind(FILE *);
int fgetpos(FILE *, fpos_t *);
int fsetpos(FILE *, const fpos_t *);
int feof(FILE *);
int ferror(FILE *);
void clearerr(FILE *);
int fileno(FILE *);
void perror(const char *);
int remove(const char *);
int rename(const char *, const char *);
int setvbuf(FILE *, char *, int, size_t);
void setbuf(FILE *, char *);
char *tmpnam(char *);
#endif
