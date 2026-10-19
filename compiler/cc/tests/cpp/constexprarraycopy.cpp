// A constexpr copy of an array of a class with a constexpr copy
// constructor (a std::array of structs holding std::optional: `constexpr
// auto componentInfo = ColorType::Model::componentInfo;`, WebCore's
// ColorInterpolation.h), its elements copied one by one in a constant
// expression; read there and at run time.
#include <array>
#include <optional>
#include <stdio.h>

enum class Type { Number, Angle };
struct Info {
    Type type;
    std::optional<int> category;
};

struct XYZModel {
    static constexpr std::array<Info, 3> componentInfo { {
        { Type::Number, std::nullopt },
        { Type::Angle, 1 },
        { Type::Number, 2 },
    } };
};
struct XYZ { using Model = XYZModel; };

template<size_t I, typename C> int kind()
{
    constexpr auto componentInfo = C::Model::componentInfo;
    if constexpr (componentInfo[I].type == Type::Angle)
        return 10 + (componentInfo[I].category ? *componentInfo[I].category : -1);
    else
        return componentInfo[I].category ? *componentInfo[I].category : -1;
}

constexpr auto copied = XYZModel::componentInfo;
static_assert(copied[2].category == 2);

int main(int argc, char **)
{
    printf("%d %d %d\n", kind<0, XYZ>(), kind<1, XYZ>(), kind<2, XYZ>());
    for (auto &info : copied)
        printf("%d %d ", (int)info.type, info.category.value_or(-5));
    printf("\n");
    return argc;
}
