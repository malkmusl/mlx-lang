// Constant tables of classes, as WebCore's color models have them: a
// static constexpr member array of a class with a constexpr constructor
// is made at compile time (the constructor writing into the array being
// made; it was left to the program's start and read as zeros by a
// static_assert); `constexpr auto info = Model::componentInfo[Index];`
// (a std::optional among its members) is a constant whose float members
// `if constexpr` compares.
#include <stdio.h>
#include <array>
#include <limits>
#include <optional>

struct W { int v; constexpr W(int x) : v(x) { } };
struct Pair { int a; W w; };
enum class Type { Number, Angle };
enum class Category { Reds, Greens, Hue };
template<typename T> struct Info {
    T min;
    T max;
    Type type;
    std::optional<Category> category;
};
struct Tables {
    static constexpr W ws[2] = { 3, 4 };
    static constexpr Pair pairs[2] = { { 1, 5 }, { 2, 6 } };
    static constexpr Info<float> infos[3] = {
        { 0, 1, Type::Number, Category::Reds },
        { 0, std::numeric_limits<float>::infinity(), Type::Number, std::nullopt },
        { 0, 360, Type::Angle, Category::Hue }
    };
};
template<typename T> struct Model;
template<> struct Model<float> {
    static constexpr std::array<Info<float>, 3> componentInfo { {
        { 0, 1, Type::Number, Category::Reds },
        { 0, std::numeric_limits<float>::infinity(), Type::Number, Category::Greens },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), Type::Angle, std::nullopt }
    } };
};
struct Color { using M = Model<float>; };

template<typename ColorType, unsigned Index> constexpr float clamped(float c)
{
    constexpr auto componentInfo = ColorType::M::componentInfo[Index];
    if constexpr (componentInfo.type == Type::Angle)
        return 1000 + c;
    if constexpr (componentInfo.min == -std::numeric_limits<float>::infinity() && componentInfo.max == std::numeric_limits<float>::infinity())
        return c;
    if constexpr (componentInfo.max == std::numeric_limits<float>::infinity())
        return c < componentInfo.min ? componentInfo.min : c;
    return c < componentInfo.min ? componentInfo.min : c > componentInfo.max ? componentInfo.max : c;
}

int main(int argc, char **argv)
{
    static_assert(Tables::ws[1].v == 4);
    static_assert(Tables::pairs[1].w.v == 6 && Tables::pairs[0].a == 1);
    static_assert(Tables::infos[2].type == Type::Angle && *Tables::infos[2].category == Category::Hue);
    constexpr auto info = Tables::infos[1];
    static_assert(!info.category.has_value());
    if constexpr (info.max == std::numeric_limits<float>::infinity()) printf("unbounded\n");
    printf("%d %d %g\n", Tables::ws[0].v, Tables::pairs[1].w.v, Tables::infos[2].max);
    printf("%g %g %g\n", clamped<Color, 0>(2.5f), clamped<Color, 1>(-3.f), clamped<Color, 2>(7.f));
    return argc;
}
