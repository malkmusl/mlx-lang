// A view's empty() (view_interface's): `range.empty()` of a transformed
// Vector given to a function taking `auto &&range` (WebCore's
// CSSCalcExecutor.h, hypot), and of a reversed one.
#include <math.h>
#include <ranges>
#include <stdio.h>
#include <vector>

double hypot(auto &&range)
{
    if (range.empty())
        return -1;
    if (range.size() == 1)
        return *range.begin();
    double sum = 0;
    for (double value : range)
        sum += value * value;
    return sqrt(sum);
}

int main(int argc, char **)
{
    std::vector<int> none;
    std::vector<int> one { 4 };
    std::vector<int> two { 3, 4 };
    auto twice = [](int v) { return v * 2.0; };
    printf("%g %g %g\n", hypot(none | std::views::transform(twice)), hypot(one | std::views::transform(twice)), hypot(two | std::views::transform(twice)));
    printf("%d %d\n", (int)(none | std::views::reverse).empty(), (int)(two | std::views::reverse).empty());
    return argc;
}
