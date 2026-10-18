// The allocation functions `new` and `delete` call (include/c++/new),
// over malloc and free. A program may define its own instead: the
// linker takes these from the archive only when nothing else defines
// them.
#include <new>
#include <stdlib.h>
#include <stdio.h>

void *operator new(size_t size) {
    void *memory = malloc(size ? size : 1);
    if (!memory) {
        fputs("operator new: out of memory\n", stderr);
        abort();
    }
    return memory;
}

void *operator new[](size_t size) { return operator new(size); }

void operator delete(void *p) { free(p); }

void operator delete[](void *p) { free(p); }
