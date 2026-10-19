// std::views::iota(first, last): the values from first to before last
// (WebCore's RenderGrid.cpp iterates a range of track indexes so: "no
// such name in the namespace"), empty when they are equal.
#include <stdio.h>
#include <ranges>

struct Range {
    unsigned first;
    unsigned last;
    unsigned begin() const { return first; }
    unsigned end() const { return last; }
};

int main(int argc, char **argv)
{
    Range range { 3, 7 };
    unsigned sum = 0;
    for (auto index : std::views::iota(range.begin(), range.end()))
        sum += index;
    int count = 0;
    for (auto index : std::views::iota(5, 5))
        count += index;
    auto view = std::views::iota(10L, 13L);
    printf("%u %d %zu %ld\n", sum, count, view.size(), *view.begin());
    return argc;
}
