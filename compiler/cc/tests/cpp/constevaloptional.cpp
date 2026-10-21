// A consteval function computed with optionals: one made from a value of
// another type (`return 3;` for an optional<unsigned>), one declared in
// an `if constexpr` and destroyed there, `!optional` (WebCore's
// ColorConversion.h hasAnalogousSet). Their constructor, destructor and
// reset are constexpr, as C++20's, and an instance whose body waits is
// read when the evaluation needs it.
#include <stdio.h>
#include <optional>
#include <array>
enum class Category { Hue, Light, Chroma };
struct Info { std::optional<Category> category; };
struct HSL { static constexpr std::array<Info, 4> componentInfo { { { Category::Hue }, { std::nullopt }, { Category::Light }, { std::nullopt } } }; };
struct LCH { static constexpr std::array<Info, 4> componentInfo { { { Category::Light }, { Category::Chroma }, { Category::Hue }, { std::nullopt } } }; };
template<typename Output, typename Input, unsigned Index>
consteval std::optional<unsigned> analogous()
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
template<typename Output, typename Input>
consteval bool hasSet()
{
    return !analogous<Output, Input, 0>() || !analogous<Output, Input, 1>() || !analogous<Output, Input, 2>() || !analogous<Output, Input, 3>();
}
int main(int argc, char**)
{
    int r = 0;
    if constexpr (hasSet<LCH, HSL>())
        r = 1;
    if constexpr (hasSet<HSL, HSL>())
        r += 10;
    printf("%d\n", r);
    return argc;
}
