/* <alloca.h>: room on the stack until the function returns. */
#ifndef _ALLOCA_H
#define _ALLOCA_H
#include <stddef.h>
#define alloca(size) __builtin_alloca(size)
#endif
