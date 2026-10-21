// `if constexpr (constexpr auto c = e)`: the declared constant is the
// condition, the branch not taken is not read, and c is in sight in both
// (WebCore's ColorConversion.h, analogousComponentIndex, over a
// std::optional of a component's category).
#include <array>
#include <optional>
#include <stdio.h>

enum class Category { Hue, Light, Chroma };
struct Info { std::optional<Category> category; };
struct HSL { static constexpr std::array<Info, 3> componentInfo { { { Category::Hue }, { std::nullopt }, { Category::Light } } }; };
struct LCH { static constexpr std::array<Info, 3> componentInfo { { { Category::Light }, { Category::Chroma }, { Category::Hue } } }; };

template<typename Output, typename Input, unsigned Index>
constexpr std::optional<unsigned> analogous()
{
    if constexpr (Index == 3)
        return 3;
    else if constexpr (constexpr auto category = Input::componentInfo[Index].category) {
        if constexpr (*category == Output::componentInfo[0].category)
            return 0;
        else if constexpr (*category == Output::componentInfo[1].category)
            return 1;
        else if constexpr (*category == Output::componentInfo[2].category)
            return 2;
        else
            return std::nullopt;
    } else
        return std::nullopt;
}

int main(int argc, char **)
{
    auto a = analogous<LCH, HSL, 0>();
    auto b = analogous<LCH, HSL, 1>();
    auto c = analogous<LCH, HSL, 2>();
    auto d = analogous<LCH, HSL, 3>();
    if constexpr (constexpr int k = sizeof(int) * 2)
        printf("k %d\n", k);
    printf("%u %d %u %u\n", *a, (int)b.has_value(), *c, *d);
    return argc;
}
