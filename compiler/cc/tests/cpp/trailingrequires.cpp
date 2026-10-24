// A trailing requires-clause naming the function's parameters (WebCore's
// CSS PrimitiveNumeric(ValueLiteral<unit> value) requires (requires {
// { Raw(value) }; })), weighed with the parameters bound; a consteval
// user-defined literal as a default member initializer.
#include <cstdio>

enum class NumberUnit : unsigned char { Number };

template<auto R> struct ValueLiteral {
    using UnitType = decltype(R);
    static constexpr auto unit = R;
    constexpr ValueLiteral operator-() { return ValueLiteral { -value }; }
    double value;
};

inline namespace Literals {
consteval ValueLiteral<NumberUnit::Number> operator""_css_number(long double value) { return ValueLiteral<NumberUnit::Number> { static_cast<double>(value) }; }
consteval ValueLiteral<NumberUnit::Number> operator""_css_number(unsigned long long value) { return ValueLiteral<NumberUnit::Number> { static_cast<double>(value) }; }
}

struct Raw {
    template<NumberUnit unitValue> constexpr Raw(ValueLiteral<unitValue> literal) : value(literal.value) { }
    constexpr Raw(double v) : value(v) { }
    double value;
};

struct Number {
    template<NumberUnit unitValue> constexpr Number(ValueLiteral<unitValue> value) requires (requires { { Raw(value) }; }) : raw { Raw { value } } { }
    Number(double v) : raw { v } { }
    Raw raw;
};

struct Ratio {
    Number numerator;
    Number denominator { 1_css_number };
};

int main()
{
    Ratio r { 3.0 };
    Number n = 2.5_css_number;
    std::printf("%g %g %g\n", r.numerator.raw.value, r.denominator.raw.value, n.raw.value);
    return 0;
}
