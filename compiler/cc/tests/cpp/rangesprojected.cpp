// std::ranges::find_if with a pointer to a member function as the
// predicate (SkiaTextureAtlas's `find_if(sizes, &IntSize::isEmpty)`),
// and std::ranges::min with the default comparison and a projection
// (TextExtraction's shortest string).
#include <stdio.h>
#include <algorithm>
#include <span>
#include <string>
#include <vector>
struct Size {
    int width, height;
    bool isEmpty() const { return width <= 0 || height <= 0; }
};
int main(int argc, char** argv)
{
    Size sizes[] = { { 2, 3 }, { 4, 0 }, { 5, 5 } };
    std::span<const Size> all { sizes };
    auto empty = std::ranges::find_if(all, &Size::isEmpty);
    std::vector<std::string> strings { "banana", "fig", "apple" };
    auto shortest = std::ranges::min(strings, { }, [](auto& string) { return string.length(); });
    auto longest = std::ranges::max(strings, { }, [](auto& string) { return string.length(); });
    auto last = std::ranges::max(strings, [](auto& a, auto& b) { return a < b; });
    printf("%d %s %s %s\n", (int) (empty - all.begin()), shortest.c_str(), longest.c_str(), last.c_str());
    return argc - 1;
}
