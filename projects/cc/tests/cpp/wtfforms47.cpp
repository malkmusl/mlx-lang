// A member function template called by its name alone in a lambda of a member function ([&], [this], [=], nested, generic): on the captured this, not the closure
#include <stdio.h>

namespace p_lamthis {
struct H {
    long v = 42;
    enum Mode { A, B };
    template <bool s, Mode m, typename F> long special(long x, Mode mm, const F &f) { return v + x + f(1) + (s ? 100 : 0) + m + mm; }
    long plain(long x) { return v + x; }
    template <typename T> long gen(T x) { return v * 2 + x; }
    long sweep(long q) {
        long a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8, i = 9, j = 10, k = 11, l = 12;
        Mode mode = B;
        auto tryIt = [&]() -> long {
            long sum = a + b + c + d + e + f + g + h + i + j + k + l;
            switch (mode) {
            case B: return sum + special<true, B>(q, B, [](int z) { return (long)z; }) + plain(q) + gen(q) + this->plain(1);
            default: return 0;
            }
        };
        return tryIt();
    }
};
int run() { H h; printf("%ld\n", h.sweep(5)); return 0; }
}

namespace p_lam2 {
struct Base { long b = 1000; template <typename T> long bgen(T x) { return b + x; } long bplain() { return b; } };
struct H : Base {
    long v = 42;
    template <typename T> long gen(T x) { return v * 2 + x; }
    template <int N> long fixed() { return v + N; }
    template <typename T> static long sgen(T x) { return x * 3; }
    long viaThis() { return [this] { return gen(1) + fixed<2>(); }(); }
    long viaCopy() { return [=] { return gen(2) + v; }(); }
    long nested() { return [&] { return [&] { return gen(3) + fixed<4>() + bgen(5) + bplain(); }(); }(); }
    long generic() { return [&](auto k) { return gen(k) + sgen(k); }(6); }
    long staticOnly() { return [] { return sgen(7); }(); }
    long member() { auto f = [&] { v += 1; return v; }; return f() + f(); }
};
int run() {
    H h;
    long a = h.viaThis(), b = h.viaCopy(), c = h.nested(), d = h.generic(), e = h.staticOnly(), f = h.member();
    printf("%ld %ld %ld %ld %ld %ld\n", a, b, c, d, e, f);
    return 0;
}
}

int main() {
    p_lamthis::run();
    p_lam2::run();
    return 0;
}
