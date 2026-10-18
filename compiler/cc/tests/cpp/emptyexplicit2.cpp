// A member function template called with an empty explicit argument
// list (`acc.template pass<>(result, a, b)`, JSBigInt's
// CombaAccumulator): its parameter's default stands, and the arrays
// given convert to the spans its parameters are.
#include <stdio.h>
#include <span>
#include <array>

template <size_t N> struct Accumulator {
    template <size_t K = 0>
    void pass(std::span<unsigned long, N * 2> r, std::span<const unsigned long, N> a, std::span<const unsigned long, N> b)
    {
        if constexpr (K < 2 * N - 1) {
            r[K] = a[K % N] * b[(K + 1) % N];
            pass<K + 1>(r, a, b);
        } else
            r[N * 2 - 1] = 7;
    }
};

template <size_t N> void run(std::span<unsigned long, N * 2> result)
{
    std::array<unsigned long, N> a;
    std::array<unsigned long, N> b;
    for (size_t i = 0; i < N; ++i) {
        a[i] = i + 1;
        b[i] = i + 2;
    }
    Accumulator<N> acc;
    acc.template pass<>(result, a, b);
}

int main(int argc, char **argv) {
    std::array<unsigned long, 4> r {};
    run<2>(std::span<unsigned long, 4>(r));
    std::array<unsigned long, 8> s {};
    run<4>(std::span<unsigned long, 8>(s));
    printf("%lu %lu %lu %lu / %lu %lu %lu\n", r[0], r[1], r[2], r[3], s[0], s[6], s[7]);
    return argc + 1;
}
