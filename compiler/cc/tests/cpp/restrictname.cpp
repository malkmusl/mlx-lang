// `restrict` is no keyword of C++: a name, of a variable, a parameter and
// a member (glslang names a parameter so); __restrict stays the qualifier.
#include <stdio.h>

struct Options
{
    int restrict;
};

static int twice(int restrict)
{
    return restrict * 2;
}

static void copy(int *__restrict to, const int *__restrict from)
{
    *to = *from;
}

int main()
{
    int restrict = 4;
    Options o    = {restrict + 1};
    int x        = 0;
    copy(&x, &o.restrict);
    printf("%d %d %d\n", twice(restrict), o.restrict, x);
    return 0;
}
