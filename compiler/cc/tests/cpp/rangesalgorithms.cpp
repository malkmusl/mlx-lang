// std::ranges' algorithms as WebCore calls them: stable_sort with `{ }`
// and a projection (GradientColorStops), upper_bound and is_sorted with
// comparisons and projections, the tests with a projection, min, max and
// minmax of two, of a list and of a range, ranges::greater.
#include <algorithm>
#include <ranges>
#include <cstdio>
#include <vector>

struct Stop {
    float offset;
    int color;
};

int main()
{
    std::vector<Stop> stops { { 0.5f, 1 }, { 0.25f, 2 }, { 0.5f, 3 }, { 0.0f, 4 } };
    std::ranges::stable_sort(stops, { }, &Stop::offset);
    for (auto& stop : stops)
        std::printf("%g:%d ", stop.offset, stop.color);
    std::printf("\n");
    std::vector<int> values { 5, 1, 4, 1, 3 };
    std::ranges::stable_sort(values, std::ranges::greater { });
    for (int v : values)
        std::printf("%d ", v);
    std::printf("\n");
    std::ranges::stable_sort(values);
    std::printf("%d %d %d\n", (int)std::ranges::is_sorted(values), (int)std::ranges::is_sorted(stops, { }, &Stop::offset), (int)std::ranges::is_sorted(values, std::ranges::greater { }));
    auto above = std::ranges::upper_bound(values, 3);
    auto stopAbove = std::ranges::upper_bound(stops, 0.25f, { }, &Stop::offset);
    std::printf("%d %d\n", (int)(above - values.begin()), (int)(stopAbove - stops.begin()));
    std::printf("%d %d %d %d\n", (int)std::ranges::any_of(stops, [](int c) { return c == 3; }, &Stop::color),
        (int)std::ranges::all_of(stops, [](float o) { return o >= 0; }, &Stop::offset),
        (int)std::ranges::none_of(stops, [](int c) { return c > 4; }, &Stop::color),
        (int)std::ranges::count_if(stops, [](float o) { return o == 0.5f; }, &Stop::offset));
    auto found = std::ranges::find(stops, 2, &Stop::color);
    std::printf("%g\n", found->offset);
    std::printf("%d %d %d %d\n", std::ranges::min(3, 7), std::ranges::max({ 3, 9, 2 }), std::ranges::max(values), std::ranges::min(values));
    int eight = 8, two = 2;
    auto [low, high] = std::ranges::minmax(eight, two);
    std::printf("%d %d %d\n", low, high, std::ranges::max(4, 6, std::ranges::greater { }));
    std::printf("%ld\n", (long)std::ranges::distance(values));
    return 0;
}
