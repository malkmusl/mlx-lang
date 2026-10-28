// A function given for a `T *` or `auto *` parameter deduces a pointer to
// a function (JSC's DFG: `[&] (..., auto* operation)` given operations).
#include <stdio.h>
long op(int x) { return x + 1; }
template<typename T> long viaTemplate(T* f) { return f(1); }
int main()
{
    auto a = [] (auto* f) { return f(2); };
    long (*p)(int) = op;
    printf("%ld %ld %ld %ld\n", viaTemplate(op), a(p), a(op), a(&op));
    return 0;
}
