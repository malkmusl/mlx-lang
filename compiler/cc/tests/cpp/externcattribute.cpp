// `extern "C" [[noreturn]] void f(...)`: attributes after the linkage
// specification (JSC's WasmIPIntSlowPaths.h).
#include <stdio.h>
#include <stdlib.h>
extern "C" [[noreturn]] void __attribute__((sysv_abi)) stop(int code) __attribute__((used));
extern "C" [[nodiscard]] int twice(int x);
extern "C" [[noreturn]] void stop(int code) { printf("stop %d\n", code); exit(0); }
extern "C" int twice(int x) { return 2 * x; }
int main() { printf("%d\n", twice(21)); stop(0); }
