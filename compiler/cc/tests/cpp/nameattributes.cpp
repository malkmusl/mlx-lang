// A name's attributes after it (`constexpr static float k [[maybe_unused]]
// = 0;`, as Skia's Tessellation.h): no array brackets.
#include <cstdio>
namespace skgpu::tess {
constexpr static float kCubicCurveType [[maybe_unused]] = 0;
constexpr static float kConicCurveType [[maybe_unused]] = 1;
}
using namespace skgpu::tess;
int main(int argc, char **argv) {
    float w = 2;
    auto pick = [&](bool conic) { return conic ? kConicCurveType + w : kCubicCurveType; };
    auto pick2 = [=](bool conic) { return conic ? kConicCurveType + w : kCubicCurveType; };
    printf("%.1f %.1f\n", pick(true), pick2(false));
    return argc;
}
