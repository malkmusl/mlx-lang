// Directives among the arguments of a function-like macro's call (C11 6.10.3p11
// leaves them undefined; GCC and Clang do them as they read the arguments,
// and JavaScriptCore's WasmBBQJIT.cpp has `EMIT_BINARY(..., #if CPU(X86_64) ...
// #endif)`): the false groups left out, the true ones kept.
#include <stdio.h>
#define PICK(name, a, b) printf("%s %d %d\n", name, a, b)
#define BLOCK(...) __VA_ARGS__
int main(void)
{
    PICK("one",
#if 1
        BLOCK(1 + 1),
#else
        BLOCK(5, 6),
#endif
        3);
    PICK("two",
#ifdef NOT_DEFINED
        10,
#elif 2 > 1
        20,
#endif
#ifndef NOT_DEFINED
        30
#endif
    );
    return 0;
}
