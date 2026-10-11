#ifndef _MLXCC_STDDEF_H
#define _MLXCC_STDDEF_H
typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef int wchar_t;
typedef struct { long long __ll; long double __ld; } max_align_t;
#define NULL ((void *)0)
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
