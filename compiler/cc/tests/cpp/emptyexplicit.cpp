// A member template called with an empty explicit argument list,
// `acc.template pass<>(r, a, b)` with `template <size_t K = 0>`: the
// only template of the name, its default completes the arguments
// (JSC's JSBigInt::multiplyComba).
#include <array>
#include <span>
#include <cstdio>
template<size_t N> struct Acc {
    template<size_t K = 0>
    void pass(std::span<unsigned long, N * 2> r, std::span<const unsigned long, N> a, std::span<const unsigned long, N> b) {
        if constexpr (K < 2 * N - 1) { r[K] = a[K % N] + b[K % N]; pass<K + 1>(r, a, b); } else r[N * 2 - 1] = 9;
    }
};
template<size_t N> void run(std::span<unsigned long, N * 2> result) {
    std::array<unsigned long, N> a { }; std::array<unsigned long, N> b { };
    for (size_t i = 0; i < N; i++) { a[i] = i + 1; b[i] = 10 * (i + 1); }
    Acc<N> acc;
    acc.template pass<>(result, a, b);
}
int main() {
    unsigned long out[4] = { };
    run<2>(std::span<unsigned long, 4>(out, 4));
    printf("%lu %lu %lu %lu\n", out[0], out[1], out[2], out[3]);
    return 0;
}
