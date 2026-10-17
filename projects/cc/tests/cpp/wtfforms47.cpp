// A member function template called by its name alone in a [&] lambda of a member function: on the captured this, not the closure
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

int main() {
    p_lamthis::run();
    return 0;
}
