// An initializer_list<T> parameter's T deduced from a braced list
// expanding a pack (`std::minmax({ values... })` of WTF's EnumValues:
// "no instance of the function template 'minmax' takes these
// arguments"), in a static member's initializer and in a function.
#include <stdio.h>
#include <algorithm>
#include <initializer_list>

enum class Color : unsigned char { Red, Green, Blue };
template <typename T> constexpr int first(std::initializer_list<T> list) { return (int)*list.begin(); }

template<typename E, E... values>
struct EnumValues {
    static constexpr auto bounds = std::minmax({ values... });
    static constexpr E min = bounds.first;
    static constexpr E max = bounds.second;
};
template<int... values>
struct Ints {
    static constexpr int f = first({ values... });
};
template<typename E, E... values>
int firstOf() { return first({ values... }); }
template<typename... T>
int sizes(T... values) { return (int)std::max({ sizeof(values)... }) + first({ values..., 0 }); }

int main(int argc, char **argv)
{
    printf("%d %d\n", Ints<3, 4>::f, firstOf<Color, Color::Green, Color::Blue>());
    printf("%d %d\n", (int)EnumValues<Color, Color::Blue, Color::Red, Color::Green>::min, (int)EnumValues<Color, Color::Green, Color::Blue>::max);
    printf("%d\n", sizes(5, 6));
    return argc;
}
