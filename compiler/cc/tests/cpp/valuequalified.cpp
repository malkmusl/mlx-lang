// `T::range` given for a value template parameter is T's member, a
// value (and with T int, no instance: the concept does not hold); of two
// templates alike, the constrained one is chosen.
#include <cstdio>
#include <concepts>

struct Range { int min; int max; };

template<Range R, typename U> struct Raw {
    static constexpr auto range = R;
    using Unit = U;
    int value;
};

template<typename T> concept IsRaw = std::derived_from<T, Raw<T::range, typename T::Unit>>;

struct Number : Raw<Range { 0, 1 }, int> { };

template<typename T> int kind(T) { return 0; }
template<IsRaw T> int kind(T) { return 1; }
template<std::integral T> int order(T) { return 2; }
template<typename T> int order(T) { return 3; }

int main(int argc, char** argv)
{
    std::printf("%d %d %d %d\n", kind(argc), kind(Number { }), (int)IsRaw<int>, (int)IsRaw<Number>);
    std::printf("%d %d\n", order(argc), order(1.5));
    return 0;
}
