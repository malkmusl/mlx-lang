// A value of a class made in braces as a template argument
// (`MetaConsumer<CSS::Integer<CSS::Range{1, CSS::Range::infinity}>>`, WebCore's
// generated CSSPropertyParsing.cpp and its easing consumer): the braces
// belong to the arguments.
#include <cstdio>
#include <limits>

namespace CSS {
struct Range {
    static constexpr double infinity = std::numeric_limits<double>::infinity();
    double min;
    double max;
    constexpr Range(double min, double max) : min(min), max(max) { }
};

template<Range R> struct Integer {
    static constexpr Range range = R;
};
}

template<typename T> struct MetaConsumer {
    static double consume() { return T::range.min; }
};

int main()
{
    auto steps = MetaConsumer<CSS::Integer<CSS::Range{1,CSS::Range::infinity}>>::consume();
    auto other = MetaConsumer<CSS::Integer<CSS::Range { 2, 5 }>>::consume();
    printf("%g %g\n", steps, other);
    return 0;
}
