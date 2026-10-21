#ifndef _MLXCC_WCHAR_H
#define _MLXCC_WCHAR_H
#include <stddef.h>
typedef unsigned int wint_t;
typedef struct { int __count; unsigned int __value; } mbstate_t;
#define WEOF 0xffffffffu
size_t wcslen(const wchar_t *);
int wcscmp(const wchar_t *, const wchar_t *);
wchar_t *wcscpy(wchar_t *, const wchar_t *);
size_t mbrtowc(wchar_t *, const char *, size_t, mbstate_t *);
size_t wcrtomb(char *, wchar_t, mbstate_t *);
#endif
