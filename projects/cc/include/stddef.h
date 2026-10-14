#ifndef _MLXCC_STDDEF_H
#define _MLXCC_STDDEF_H
typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef int wchar_t;
typedef struct { long long __ll; long double __ld; } __attribute__((aligned(16))) max_align_t;
#define NULL ((void *)0)
#define offsetof(type, member) __builtin_offsetof(type, member)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef typeof(nullptr) nullptr_t;
#define unreachable() __builtin_unreachable()
#define __STDC_VERSION_STDDEF_H__ 202311L
#endif
#endif
