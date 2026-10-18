// Explicit specializations of a class template instance's members named
// through an alias of the instance (`template <> Color4f
// Color4f::FromColor(uint32_t);`, as Skia's SkColor.h), defined before
// the template's own out-of-class definition of the member.
#include <cstdio>
#include <cstdint>
enum AlphaType { kPremul, kUnpremul };
template <AlphaType at> struct RGBA4f {
    float r, g, b, a;
    static RGBA4f FromColor(uint32_t c);
    uint32_t toColor() const;
    template <typename T> T as() const { return T(r); }
};
using Color4f = RGBA4f<kUnpremul>;
template <> Color4f Color4f::FromColor(uint32_t);
template <> uint32_t Color4f::toColor() const;

template <> Color4f Color4f::FromColor(uint32_t c) {
    return Color4f{ ((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, (c >> 24) / 255.0f };
}
template <> uint32_t Color4f::toColor() const {
    return ((uint32_t)(a * 255) << 24) | ((uint32_t)(r * 255) << 16) | ((uint32_t)(g * 255) << 8) | (uint32_t)(b * 255);
}
template <AlphaType at> RGBA4f<at> RGBA4f<at>::FromColor(uint32_t c) { return RGBA4f{1, 1, 1, (float)c}; }

int main(int argc, char **argv) {
    Color4f c = Color4f::FromColor(0xff336699u);
    printf("%.3f %.3f %.3f %.3f %08x\n", c.r, c.g, c.b, c.a, c.toColor());
    RGBA4f<kPremul> p = RGBA4f<kPremul>::FromColor(7);
    printf("%.1f %d\n", p.a, c.as<int>());
    return argc;
}
