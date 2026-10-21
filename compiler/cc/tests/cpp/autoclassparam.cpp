// A partial specialization's `auto` parameter deduced from a class-type
// argument (C++20), passed on to a template taking that class.
#include <cstdio>

struct Range {
    double min;
    double max;
    constexpr Range(double a, double b) : min(a), max(b) { }
    constexpr bool operator==(const Range&) const = default;
};
inline constexpr Range All { -1000, 1000 };
inline constexpr Range Unit { 0, 1 };

template<Range R = All, typename V = float> struct Length { static constexpr double low = R.min; V v; };
template<Range R = All, typename V = float> struct Percentage { static constexpr double high = R.max; V v; };

template<typename> struct Mapping;
template<auto R, typename V> struct Mapping<Length<R, V>> {
    using Dimension = Length<R, V>;
    using Percent = Percentage<R, V>;
};

int main(int argc, char** argv)
{
    using M = Mapping<Length<Unit, double>>;
    M::Percent p { 0.5 };
    std::printf("%g %g %g\n", M::Dimension::low, M::Percent::high, p.v * argc);
    return 0;
}
