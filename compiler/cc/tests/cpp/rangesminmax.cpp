// std::ranges::minmax of a range, a view too (`std::ranges::minmax(
// corners | std::views::transform(wAt))` in WebCore's
// SkiaCompositingLayerOverlapRegions.cpp: "no instance of the function
// template 'minmax'"), beside those of two values and of a list.
#include <stdio.h>
#include <algorithm>
#include <array>
#include <ranges>
#include <vector>

struct Point { float x; float y; };

int main(int argc, char **argv)
{
    const std::array<Point, 4> corners { Point { 1, 2 }, Point { -3, 4 }, Point { 5, -6 }, Point { 0, 0 } };
    float a = 2, b = 1, c = 0.5f;
    auto wAt = [&](const Point &p) { return a * p.x + b * p.y + c; };
    auto [least, most] = std::ranges::minmax(corners | std::views::transform(wAt));
    std::vector<int> values { 4, 9, -2, 7 };
    auto bounds = std::ranges::minmax(values);
    int three = 3, one = 1;
    auto two = std::ranges::minmax(three, one);
    auto list = std::ranges::minmax({ 8, 2, 5 });
    printf("%g %g %d %d %d %d %d %d\n", least, most, bounds.min, bounds.max, two.min, two.max, list.min, list.max);
    return argc;
}
