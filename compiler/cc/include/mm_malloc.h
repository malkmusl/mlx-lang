/* <mm_malloc.h>: _mm_malloc and _mm_free (<xmmintrin.h> includes it). */
#ifndef _MLXCC_MM_MALLOC_H
#define _MLXCC_MM_MALLOC_H
#include <stdlib.h>
static inline void *_mm_malloc(size_t size, size_t alignment) {
    void *p;
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    return posix_memalign(&p, alignment, size) == 0 ? p : 0;
}
static inline void _mm_free(void *p) { free(p); }
#endif
