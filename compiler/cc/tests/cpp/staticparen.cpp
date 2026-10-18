// A static local initialized by `= (expression) ...`: the parenthesis is
// the value's own, not `T x(args)` (SIMDe's float16 conversion), next to
// `thread_local T x(args)`.
#include <cstdint>
#include <cstdio>
unsigned f(unsigned v) {
    static const uint32_t a = UINT32_C(255) << 23;
    static const uint32_t b = (UINT32_C(127) + UINT32_C(16)) << 23;
    static const uint32_t c = (UINT32_C(127) - UINT32_C(15)) + UINT32_C(1);
    return v + a + b + c;
}
static int serial = 4;
int g() { thread_local int id(++serial); return id; }
int main() { printf("%u %d %d\n", f(1), g(), g()); return 0; }
