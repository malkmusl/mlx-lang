// C++20's template parameters of a class type (WebCore's CSS
// `template <Range R = All> struct Number`, `Number<Nonnegative>`): the
// argument a constant object, instances equal by value, deduced from an
// instance's type, read in constant expressions and at run time.
#include <cstdio>
#include <limits>

enum class Behavior : unsigned char { Default, Clamp };

struct Range {
    static constexpr auto infinity = std::numeric_limits<double>::infinity();
    double min { -infinity };
    double max { infinity };
    Behavior behavior { Behavior::Default };
    constexpr Range(double min, double max, Behavior behavior = Behavior::Default) : min { min }, max { max }, behavior { behavior } { }
    constexpr bool operator==(const Range&) const = default;
};

inline constexpr auto All = Range { -Range::infinity, Range::infinity };
inline constexpr auto Nonnegative = Range { 0, Range::infinity };
inline constexpr auto Clamped = Range { 0, 1, Behavior::Clamp };

template<Range R = All, typename V = double> struct Number {
    static constexpr auto range = R;
    V value;
    constexpr bool fits() const { return value >= R.min && value <= R.max; }
};

template<Range R> double low(const Number<R>&) { return R.min; }
template<Range R, typename V> V clampTo(const Number<R, V>& n)
{
    if constexpr (R.behavior == Behavior::Clamp)
        return n.value < R.min ? R.min : (n.value > R.max ? R.max : n.value);
    else
        return n.value;
}

static_assert(Number<Nonnegative>::range.min == 0);
static_assert(Number<>::range == All);
static_assert(!(Number<Clamped> { 2 }.fits()));

int main()
{
    Number<Nonnegative> a { 3 };
    Number<Clamped> b { 5 };
    Number<Range { 1, 5 }> c { 7 };
    Range copy = Number<Clamped>::range;
    std::printf("%g %g %g %g %g %g\n", low(a), low(b), low(c), clampTo(b), clampTo(a), copy.max);
    return 0;
}
