// Arguments aligned to 16 on the stack (__int128) whose values come from calls: the padding before the next argument kept
#include <stdio.h>

namespace p_i128 {
typedef __int128 Int128;
static Int128 absInt128(Int128 v) { return v < 0 ? -v : v; }
struct D {
    long a, b, c, d, e, f, g, h; Int128 us, ns;
    D(long a, long b, long c, long d, long e, long f, long g, long h, Int128 us, Int128 ns) : a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h), us(us), ns(ns) {}
};
static long sum(long a, long b, long c, long d, long e, long f, Int128 x, long g, Int128 y) { return a + b + c + d + e + f + (long)x + g + (long)y; }
D make(const D &d) { return D(d.a, d.b, d.c, d.d, d.e, d.f, d.g, d.h, absInt128(d.us), absInt128(d.ns)); }
int run() {
    D d(1, 2, 3, 4, 5, 6, 7, 8, -9, -10);
    D e = make(d);
    printf("%ld %ld %ld %ld\n", e.h, (long)e.us, (long)e.ns, sum(1, 2, 3, 4, 5, 6, 7, 8, 9));
    return 0;
}
}

namespace p_i128c {
typedef __int128 Int128;
static Int128 neg(Int128 v) { return -v; }
static long sum(long a, long b, long c, long d, long e, long f, long g, Int128 x) { return a + b + c + d + e + f + g + (long)x; }
int run() {
    Int128 v = 5;
    printf("%ld\n", sum(1, 2, 3, 4, 5, 6, 7, neg(v)));
    return 0;
}
}

int main() {
    p_i128::run();
    p_i128c::run();
    return 0;
}
