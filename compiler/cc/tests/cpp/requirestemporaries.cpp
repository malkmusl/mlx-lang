// A requires-expression whose requirement makes a temporary of a class
// with a destructor (std::optional), after `&&` in a concept (WebCore's
// CSS::UnitEnum): still a constant.
#include <stdio.h>
#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

enum class Category : unsigned char { Length };
enum class LengthUnit : unsigned char { Px, Em };
template<typename> struct UnitTraits;
template<> struct UnitTraits<LengthUnit> {
    static constexpr auto count = 2;
    static constexpr auto canonical = LengthUnit::Px;
    static constexpr auto category = Category::Length;
    static constexpr std::optional<LengthUnit> validate(int unit) { return unit == 1 ? std::optional<LengthUnit> { LengthUnit::Px } : std::nullopt; }
};

template<typename T> concept UnitEnum = std::is_enum_v<T> && requires {
    requires std::integral<decltype(UnitTraits<T>::count)>;
    requires std::same_as<decltype(UnitTraits<T>::canonical), const T>;
    requires std::same_as<decltype(UnitTraits<T>::category), const Category>;
    { UnitTraits<T>::validate(std::declval<int>()) } -> std::same_as<std::optional<T>>;
};

static_assert(UnitEnum<LengthUnit>);
static_assert(!UnitEnum<Category>);
static_assert(!UnitEnum<int>);

template<UnitEnum E> int first() { return (int)UnitTraits<E>::canonical + UnitTraits<E>::count; }

int main(int argc, char **argv)
{
    printf("%d %d\n", (int)UnitEnum<LengthUnit>, first<LengthUnit>());
    return argc - 2;
}
