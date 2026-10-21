#include <cstdio>

enum class Behavior { Default, Clamp };

struct Range {
    double min;
    double max;
    Behavior lower = Behavior::Default;
    Behavior upper = Behavior::Default;
    constexpr bool operator==(const Range&) const = default;
};

inline constexpr auto All = Range { -1000, 1000 };
inline constexpr auto Unit = Range { 0, 1, Behavior::Default, Behavior::Clamp };
inline constexpr auto Positive = Range { 1, 1000 };

template<Range R = All, typename V = double> struct CSSNumber {
    using Value = V;
    static constexpr auto range = R;
};

template<typename CSS> struct Primitive {
    using Value = typename CSS::Value;
    static constexpr auto range = CSS::range;
    Value value { 0 };

    constexpr Primitive(Value v) : value { v } { }

    constexpr bool isZero() const requires (range.min <= 0 && range.max >= 0) { return !value; }
    constexpr bool isPositive() const requires (range.max > 0) { return value > 0; }
};

template<Range R = All, typename V = double> struct Number : Primitive<CSSNumber<R, V>> {
    using Base = Primitive<CSSNumber<R, V>>;
    using Base::Base;
};

struct Grayscale {
    using Parameter = Number<Unit>;
    Parameter value;
    bool isIdentity() const { return value.isZero(); }
};

template<typename T> concept HasIsZero = requires(const T& t) { t.isZero(); };

int main(int argc, char** argv)
{
    Grayscale g { { 0.0 } };
    Grayscale h { { argc * 0.25 } };
    Number<Positive> p { 2.0 };
    std::printf("%d %d %d\n", (int)g.isIdentity(), (int)h.isIdentity(), (int)p.isPositive());
    std::printf("%d %d\n", (int)HasIsZero<Number<Unit>>, (int)HasIsZero<Number<Positive>>);
    return 0;
}
