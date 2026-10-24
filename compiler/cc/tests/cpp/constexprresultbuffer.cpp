// A constexpr function returning a class over 16 bytes gives it to the
// caller's buffer, whose address comes as a hidden first parameter: the
// interpreter binds the written parameters to the arguments after it
// (`makeBox(i)` returning `Box(i)`, its 16-byte Info copied from `i`,
// had `i` unbound). And std::make_tuple and std::get are constexpr, as
// C++14 has them: WebCore's `std::get<Index>(Descriptor::components)` of a
// constexpr tuple of color components in a constexpr local.
#include <cstdio>
#include <limits>
#include <tuple>

enum class Type { Number, Angle };
struct Info {
    int symbol;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    double numberMultiplier = 1.0;
    Type type = Type::Number;
};
struct Box { Info head; int tail; constexpr Box(const Info& h) : head(h), tail(0) {} };
struct Pair { Info head; Info second; constexpr Pair(Info h, const Info& s) : head(h), second(s) {} };

constexpr Box makeBox(const Info& i) { return Box(i); }
constexpr Box makeBoxNamed(const Info& i) { Box b(i); return b; }
constexpr Pair makePair(Info a, const Info& b) { return Pair(a, b); }
constexpr Info one { .symbol = 9, .min = 0.0, .max = 1.0 };
static_assert(makeBox(one).head.symbol == 9 && makeBox(one).tail == 0);
static_assert(makeBoxNamed(Info { .symbol = 4 }).head.symbol == 4);
static_assert(makePair(one, Info { .symbol = 5 }).second.symbol == 5 && makePair(one, Info { .symbol = 5 }).head.symbol == 9);

constexpr auto Alpha = Info { .symbol = 9, .min = 0.0, .max = 1.0 };
struct Descriptor {
    static constexpr auto components = std::make_tuple(
        Info { .symbol = 1, .min = 0.0, .max = 255.0, .numberMultiplier = 1.0 / 255.0 },
        Info { .symbol = 2, .type = Type::Angle },
        Info { .symbol = 3 },
        Alpha
    );
};

template<typename D, unsigned Index> float convert(double number)
{
    constexpr auto info = std::get<Index>(D::components);
    constexpr auto multiplier = info.numberMultiplier;
    constexpr auto min = info.min * info.numberMultiplier;
    constexpr auto max = info.max * info.numberMultiplier;
    if constexpr (info.type == Type::Angle)
        return (float)(number - 360.0 * (int)(number / 360.0));
    else if constexpr (info.min == -std::numeric_limits<double>::infinity() && info.max == std::numeric_limits<double>::infinity())
        return (float)(number * multiplier);
    else
        return (float)(number * multiplier < min ? min : number * multiplier > max ? max : number * multiplier);
}

int main()
{
    printf("%g %g %g %g\n", convert<Descriptor, 0>(510), convert<Descriptor, 1>(400), convert<Descriptor, 2>(7), convert<Descriptor, 3>(2));
    constexpr auto pair = std::make_tuple(7, 8L);
    printf("%d %ld %d\n", std::get<0>(pair), std::get<1>(pair), (int)std::get<int>(pair));
    Box box = makeBox(Info { .symbol = 6 });
    printf("%d %d\n", box.head.symbol, box.tail);
    return 2;
}
