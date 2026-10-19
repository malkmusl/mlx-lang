#ifndef _MLXCC_REGEX_H
#define _MLXCC_REGEX_H
#include <stddef.h>
#include <sys/types.h>
/* glibc's layout: regex_t is 64 bytes with re_nsub at offset 48, and
   regoff_t an int, so that a program compiled here runs on glibc's
   regcomp and regexec as on mlxlibc's (compiler/libc/regex.mlx). */
typedef int regoff_t;
typedef unsigned long reg_syntax_t;
typedef struct re_pattern_buffer {
    void *__buffer;
    size_t __allocated;
    size_t __used;
    reg_syntax_t __syntax;
    char *__fastmap;
    unsigned char *__translate;
    size_t re_nsub;
    unsigned __flags;
} regex_t;
typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;
#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NEWLINE 4
#define REG_NOSUB 8
#define REG_NOTBOL 1
#define REG_NOTEOL 2
#define REG_STARTEND 4
#define REG_NOMATCH 1
#define REG_BADPAT 2
#define REG_ECOLLATE 3
#define REG_ECTYPE 4
#define REG_EESCAPE 5
#define REG_ESUBREG 6
#define REG_EBRACK 7
#define REG_EPAREN 8
#define REG_EBRACE 9
#define REG_BADBR 10
#define REG_ERANGE 11
#define REG_ESPACE 12
#define REG_BADRPT 13
#define REG_EEND 14
#define REG_ESIZE 15
#define REG_ERPAREN 16
#ifdef __cplusplus
extern "C" {
#endif
int regcomp(regex_t *, const char *, int);
int regexec(const regex_t *, const char *, size_t, regmatch_t *, int);
size_t regerror(int, const regex_t *, char *, size_t);
void regfree(regex_t *);
#ifdef __cplusplus
}
#endif
#endif
