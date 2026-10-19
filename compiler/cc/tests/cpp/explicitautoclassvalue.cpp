// A class's value given explicitly for an `auto` parameter of a function
// template (`value.m_value.template minimumValueForPrimitiveDataWith
// LazyMaximum<StyleType::range, LayoutUnit, LayoutUnit>(...)` of
// WebCore's PrimitiveData, its range a CSS::Range): the instance takes
// it ("no instance of the function template").
#include <stdio.h>
#include <functional>
#include <type_traits>
#include <utility>

template<typename Functor, typename Signature> concept Invocable = requires(std::decay_t<Functor>&& f, std::function<Signature> expected) {
    { expected = std::move(f) };
};
struct ZoomNeeded { };
struct ZoomFactor { float value; };
struct Range { double min; double max; };
constexpr Range All { -1, 1 };

struct PrimitiveData {
    float m_floatValue = 2;
    template<auto range, typename ReturnType, typename MaximumType>
    ReturnType minimum(int kind, const Invocable<MaximumType()> auto& lazy, ZoomNeeded) const;
    template<auto range, typename ReturnType, typename MaximumType>
    ReturnType minimum(int kind, const Invocable<MaximumType()> auto& lazy, ZoomFactor) const;
};
template<auto range, typename ReturnType, typename MaximumType>
ReturnType PrimitiveData::minimum(int kind, const Invocable<MaximumType()> auto& lazy, ZoomNeeded) const
{
    return ReturnType(m_floatValue + lazy() + kind + range.max);
}
template<auto range, typename ReturnType, typename MaximumType>
ReturnType PrimitiveData::minimum(int kind, const Invocable<MaximumType()> auto& lazy, ZoomFactor zoom) const
{
    return ReturnType(m_floatValue * zoom.value + lazy() + kind);
}

template<auto R> struct LP { static constexpr auto range = R; PrimitiveData m_value; };

int main(int argc, char **argv)
{
    LP<All> value;
    long maximumValue = 100;
    long a = value.m_value.template minimum<LP<All>::range, long, long>(1, [&] { return long(maximumValue); }, ZoomFactor { 3 });
    long b = value.m_value.template minimum<LP<All>::range, long, long>(1, [&] { return long(maximumValue); }, ZoomNeeded { });
    printf("%ld %ld\n", a, b);
    return argc;
}
