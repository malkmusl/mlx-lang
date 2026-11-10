// The C++ reference of the mathutil differential test: the same inputs
// the Mlx driver uses, the packed value and the round-tripped floats' bits.
#include "common/mathutil.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
// debug.cpp is not linked: the stream ASSERT swallows into (never, under NDEBUG).
namespace gl { namespace priv { std::ostream *gSwallowStream = nullptr; } }
static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
int main() {
    float in[6][3] = {{0, 0, 0}, {1, 0.5f, 0.25f}, {0.001f, 0.002f, 0.003f}, {65408.0f, 1, 2}, {100, 200, 300}, {0.75f, 0, 12.5f}};
    for (auto &rgb : in) {
        unsigned p = gl::convertRGBFloatsTo999E5(rgb[0], rgb[1], rgb[2]);
        float r, g, b;
        gl::convert999E5toRGBFloats(p, &r, &g, &b);
        printf("%u %u %u %u\n", p, bits(r), bits(g), bits(b));
    }
    return 0;
}
