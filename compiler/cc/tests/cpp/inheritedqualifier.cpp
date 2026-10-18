// A base's typedef of a class as a qualifier in the derived class
// (`Base::Fixed`, WebCore's PrimitiveNumericOrKeyword's Base in the
// classes derived from it), in member functions and their lambdas.
#include <stdio.h>

namespace style {
template<class T> struct Percentage { T value; };
template<class T> struct LengthPercentage { using Percentage = style::Percentage<T>; };

template<class N, int... Keywords> struct NumericOrKeyword;
template<class T, int... Keywords> struct NumericOrKeyword<LengthPercentage<T>, Keywords...> {
    using Base = NumericOrKeyword<LengthPercentage<T>, Keywords...>;
    using Numeric = LengthPercentage<T>;
    using Percentage = typename Numeric::Percentage;
    using Fixed = int;
    NumericOrKeyword(int x) : x(x) { }
    int x;
};

struct BackgroundSize : NumericOrKeyword<LengthPercentage<float>, 1> {
    using Base::Base;
    int sum() const
    {
        Base::Fixed fixed = 4;
        auto plus = [](const Base::Fixed& value) { return value + 1; };
        auto truncated = [](const Base::Percentage& percentage) { return (int)percentage.value; };
        Base::Percentage half { 2.5f };
        return fixed + plus(1) + truncated(half) + x;
    }
};
}

int main(int argc, char **argv)
{
    style::BackgroundSize size(3);
    printf("%d\n", size.sum());
    return argc - 2;
}
