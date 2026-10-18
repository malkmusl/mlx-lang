// Abbreviated function templates where the name is an operator's
// (`operator+=(std::convertible_to<double> auto const &)`) and in friend
// declarations (`friend Value operator+(const Value &, C auto const &)`),
// as WebCore's CSS ValueLiteral has them; constrained and plain `auto`
// parameters, a pack of them.
#include <cstdio>
#include <concepts>

template<typename T> concept Small = sizeof(T) <= 2;

struct Value {
    double value;
    constexpr Value& operator+=(std::convertible_to<double> auto const& rhs)
    {
        value += static_cast<double>(rhs);
        return *this;
    }
    friend constexpr Value operator+(const Value& lhs, std::convertible_to<double> auto const& rhs)
    {
        return Value { lhs.value + static_cast<double>(rhs) };
    }
};

static int twice(Small auto x) { return x * 2; }
static long plain(auto x, const auto& y) { return static_cast<long>(x) + static_cast<long>(y); }
static double total(std::integral auto... values) { return (0.0 + ... + values); }

int main()
{
    Value v { 1.5 };
    v += 2;
    v += 0.25f;
    Value w = v + 3;
    short s = 21;
    std::printf("%g %g %d %ld %g\n", v.value, w.value, twice(s), plain(3, 4.0), total(1, 2, 3));
    return 0;
}
