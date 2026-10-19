// A class's value given for an `auto` template parameter of a class
// template (`LengthPercentage<CSS::All, float>` of WebCore's `template<
// auto R, typename V>`): an object of its class, as for a parameter of
// that class ("not a compile-time constant", read as an integer);
// `static constexpr auto range = R;` then a template argument.
#include <stdio.h>

enum class Zoom { Default, Unzoomed };
struct Range { double min; double max; Zoom zoomOptions = Zoom::Default; struct Inner { static constexpr int value = 7; }; };
constexpr Range All { -1, 1 };
constexpr Range Unz { -1, 1, Zoom::Unzoomed };

struct Data {
    template<Range R, typename A, typename B, typename F> A minimum(int kind, const F& f, int zoom) const { return A(R.max * 100 + f() + kind + zoom); }
};
template<auto R, typename V> struct LengthPercentage {
    static constexpr auto range = R;
    Data m_value;
    int evaluationKind() const { return 3; }
};
template<typename S, typename Result> struct EvaluationMinimum;
template<auto R, typename V, typename Result> struct EvaluationMinimum<LengthPercentage<R, V>, Result> {
    using StyleType = LengthPercentage<R, V>;
    auto operator()(const StyleType& value, Result maximumValue, int zoom) -> Result
        requires (StyleType::range.zoomOptions == Zoom::Default)
    {
        return value.m_value.template minimum<StyleType::range, long, long>(value.evaluationKind(), [&] { return long(maximumValue); }, zoom);
    }
    auto operator()(const StyleType& value, Result maximumValue, long zoom) -> Result
        requires (StyleType::range.zoomOptions == Zoom::Unzoomed)
    {
        return value.m_value.template minimum<StyleType::range, long, long>(value.evaluationKind(), [&] { return long(maximumValue) * 2; }, zoom);
    }
};

int main(int argc, char **argv)
{
    LengthPercentage<All, float> a;
    LengthPercentage<Unz, float> b;
    printf("%ld %ld\n", EvaluationMinimum<LengthPercentage<All, float>, long> { }(a, 5, 1), EvaluationMinimum<LengthPercentage<Unz, float>, long> { }(b, 5, 1L));
    printf("%d %d\n", (int)LengthPercentage<Unz, int>::range.zoomOptions, (int)decltype(LengthPercentage<All, int>::range)::Inner::value);
    return argc;
}
