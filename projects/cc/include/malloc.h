/* <malloc.h>: the allocator's extras of glibc systems. */
#ifndef _MLXCC_MALLOC_H
#define _MLXCC_MALLOC_H
#include <stdlib.h>
size_t malloc_usable_size(void *);
int malloc_trim(size_t);
void *memalign(size_t, size_t);
#endif
