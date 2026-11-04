// A function declared `extern "C"` after a C++ overload of its name, of
// another signature, and defined without the linkage specification, is
// C's (JavaScriptCore's WebAssemblySuspending.cpp: the host function
// JSC_DECLARE_HOST_FUNCTION declares, then `extern "C" void *
// runWebAssemblySuspendingFunction(...)` that the in-place interpreter's
// assembly calls by its C name).
#include <stdio.h>

namespace engine {
long run(int, char);
extern "C" void* run(int count, long* values);
void* run(int count, long* values) { return values + count; }
long run(int a, char b) { return a + b; }
}

extern "C" void* run(int, long*);

int main()
{
    long values[4] = { 1, 2, 3, 4 };
    long* last = static_cast<long*>(run(3, values));
    printf("%ld %ld\n", *last, engine::run(40, 2));
    return 0;
}
