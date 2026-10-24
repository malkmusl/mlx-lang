// WebCore's CSS numeric literals (`0_css_px`): a value parameter whose
// type is a parameter before it (`template<UnitEnum E, E unit>
// Primitive(ValueLiteral<unit>)`, E deduced from the value), its trailing
// requires-clause kept with the constrained parameters', and a class
// inheriting the constructor (`using Base::Base;`) constructible and
// convertible through it.
#include <stdio.h>
#include <concepts>
#include <type_traits>

enum class LengthUnit { Px, Em };
enum class AngleUnit { Deg };
template<class E> concept UnitEnum = std::is_enum_v<E>;
template<auto unit> requires UnitEnum<decltype(unit)> struct ValueLiteral {
    static constexpr auto unitValue = unit;
    double value;
};

struct LengthRaw {
    LengthUnit unit;
    double value;
    constexpr LengthRaw(double v) : unit(LengthUnit::Em), value(v) { }
    constexpr LengthRaw(ValueLiteral<LengthUnit::Px> l) : unit(LengthUnit::Px), value(l.value) { }
};

template<class R> struct Primitive {
    using Raw = R;
    template<UnitEnum E, E unitValue>
    constexpr Primitive(ValueLiteral<unitValue> literal) requires (requires { { Raw(literal) }; })
        : raw { Raw { literal } }
    {
    }
    constexpr Primitive(double v) : raw { v } { }
    Raw raw;
};

struct LengthPercentage : Primitive<LengthRaw> {
    using Base = Primitive<LengthRaw>;
    using Base::Base;
};

template<class E, E unit> constexpr int unitIndex(ValueLiteral<unit>) { return (int)unit + 10; }

using Px = ValueLiteral<LengthUnit::Px>;
using Em = ValueLiteral<LengthUnit::Em>;
static_assert(std::is_constructible_v<Primitive<LengthRaw>, Px>);
static_assert(!std::is_constructible_v<Primitive<LengthRaw>, Em>);
static_assert(std::is_constructible_v<LengthPercentage, Px>);
static_assert(!std::is_constructible_v<LengthPercentage, Em>);
static_assert(std::convertible_to<Px, LengthPercentage>);
static_assert(!std::convertible_to<Em, LengthPercentage>);
static_assert(!std::convertible_to<ValueLiteral<AngleUnit::Deg>, LengthPercentage>);
static_assert(unitIndex(Em { 1 }) == 11);

int main(int argc, char **argv)
{
    LengthPercentage length = Px { 2.5 };
    LengthPercentage other { 4.0 };
    printf("%d %d %d %d\n", (int)(length.raw.value * 2), (int)length.raw.unit, (int)other.raw.unit, unitIndex(Px { 0 }));
    return argc - 2;
}
