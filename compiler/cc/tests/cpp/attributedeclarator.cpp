// GNU attributes opening a parenthesized declarator (libxml2's
// xmlmemory.h: `typedef void *(LIBXML_ATTR_ALLOC_SIZE(1) XMLCALL
// *xmlMallocFunc)(size_t size);`, "typedef name omitted").
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
typedef void *(__attribute__((alloc_size(1))) *MallocFunc)(size_t size);
typedef int (__attribute__((unused)) *Twice)(int);
static int twice(int x) { return 2 * x; }
int (__attribute__((unused)) value) = 7;

int main(int argc, char **)
{
    MallocFunc f = malloc;
    void *p = f(16);
    Twice t = twice;
    printf("%d %d %d\n", p != nullptr, t(21), value);
    free(p);
    return argc;
}
