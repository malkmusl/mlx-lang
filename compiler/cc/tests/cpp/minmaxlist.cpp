// std::minmax of an initializer_list (Skia's `auto [lineTop, lineBottom] =
// std::minmax({pts[0].fY, pts[1].fY})`).
#include <algorithm>
#include <cstdio>

struct Point { float fX, fY; };

int main(int argc, char **argv) {
    Point pts[2] = {{1, 7.5f}, {2, -3}};
    auto [lineTop, lineBottom] = std::minmax({pts[0].fY, pts[1].fY});
    auto [low, high] = std::minmax({4, 9, -2, 9, 3});
    auto pair = std::minmax(5, 2);
    printf("%.1f %.1f %d %d %d %d\n", lineTop, lineBottom, low, high, pair.first, pair.second);
    return argc;
}
