/* ISO C90 as mlx-cc reads it with -std=c89: no __STDC_VERSION__,
   __STRICT_ANSI__ set, `inline` and `restrict` are plain identifiers,
   `linux` and `unix` are not macros, comments are these. gcc -std=c89
   printed the expected output. */
#include <stdio.h>

static int inline = 3;
static int restrict = 4;
static int typeof = 5;

#ifdef __STDC_VERSION__
#error C90 has no __STDC_VERSION__
#endif
#ifndef __STRICT_ANSI__
#error strict
#endif
#if defined(linux) || defined(unix)
#error gnu names
#endif

struct pair { int first; int second; };

static struct pair make(int a, int b)
{
    struct pair p;
    p.first = a;
    p.second = b;
    return p;
}

int main(void)
{
    struct pair p = make(inline, restrict);
    int linux = 1;
    int unix = 2;
    printf("%d %d %d %d %d\n", p.first, p.second, typeof, linux, unix);
    printf("%d\n", __STDC__);
    return 0;
}
