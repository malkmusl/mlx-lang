// A class inheriting its base's constructors is no aggregate for the
// overloads of a braced list: `return { { r, g, b } };` makes WebCore's
// SRGBA<uint8_t> (`using RGBAType<...>::RGBAType;`) of an optional
// through RGBAType(T, T, T, T = 255).
#include <cstdio>
#include <optional>
template<typename T> struct RGBAType {
    constexpr RGBAType(T r, T g, T b, T a = 255) : red(r), green(g), blue(b), alpha(a) { }
    T red, green, blue, alpha;
};
template<typename T> struct SRGBA : RGBAType<T> {
    using RGBAType<T>::RGBAType;
};
std::optional<SRGBA<unsigned char>> parse(int which)
{
    if (!which)
        return std::nullopt;
    return { { static_cast<unsigned char>(which), static_cast<unsigned char>(which * 2), static_cast<unsigned char>(which * 3) } };
}
int main() {
    auto c = parse(4);
    printf("%d %d %d %d %d\n", (int)c.has_value(), c->red, c->green, c->blue, c->alpha);
    printf("%d\n", (int)parse(0).has_value());
}
