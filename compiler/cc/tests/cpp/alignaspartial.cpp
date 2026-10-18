// Partial specializations with alignas between the class key and the
// name (`struct alignas(4*sizeof(T)) Vec<4,T>`, as Skia's SkVx.h).
#include <cstdio>

namespace vx {
template <int N, typename T> struct alignas(N * sizeof(T)) Vec;

template <int N, typename T> struct alignas(N * sizeof(T)) Vec {
    Vec<N / 2, T> lo, hi;
    T sum() const { return lo.sum() + hi.sum(); }
};

template <typename T> struct alignas(2 * sizeof(T)) Vec<2, T> {
    Vec<1, T> lo, hi;
    T sum() const { return lo.sum() + hi.sum(); }
};

template <typename T> struct Vec<1, T> {
    T val;
    T sum() const { return val; }
};

struct Holder {
    using F4 = vx::Vec<4, float>;
    F4 v;
};
}

int main(int argc, char **argv) {
    vx::Holder h;
    h.v.lo.lo.val = 1;
    h.v.lo.hi.val = 2;
    h.v.hi.lo.val = 3;
    h.v.hi.hi.val = 4;
    vx::Vec<8, double> d{};
    printf("%.0f %zu %zu %zu %.0f\n", h.v.sum(), sizeof(h.v), alignof(vx::Vec<2, float>), alignof(vx::Vec<8, double>), d.sum());
    return argc;
}
