// A base's member type hides a namespace's name of the same spelling in
// the derived class (WebCore's LineHeight: `Percentage` is its base's
// typedef there, not the class template Style::Percentage).
#include <stdio.h>

namespace style {
template<class T> struct Percentage { T value; };
struct Fixed { int unused; };

template<class T> struct NumericOrKeyword {
    struct Fixed { int value; };
    using Percentage = style::Percentage<T>;
    Percentage percentage { 25 };
    Fixed fixed { 3 };
};

struct LineHeight : NumericOrKeyword<int> {
    Percentage half() const { return Percentage { percentage.value / 2 }; }
    int sum() const
    {
        auto take = [&](const Percentage& p) { return p.value; };
        auto plus = [](const Fixed& f) { return f.value + 1; };
        return take(half()) + plus(fixed);
    }
};
}

// A dependent base's member is no name in the template (the namespace
// css is meant in Style::Number's base, though the base has a member css).
namespace css {
template<class T> struct Number { using Raw = T; };
}
namespace style {
template<class C> struct Primitive {
    using css = C;
    typename css::Raw raw { 7 };
};
template<class T> struct Number : Primitive<css::Number<T>> {
    using Base = Primitive<css::Number<T>>;
    typename Base::css::Raw twice() const { return this->raw * 2; }
};
}

int main(int argc, char **argv)
{
    style::LineHeight height;
    style::Percentage<float> outside { 1.5f };
    style::Number<int> number;
    printf("%d %d %d\n", height.sum(), (int)(outside.value * 2), number.twice());
    return argc - 2;
}
