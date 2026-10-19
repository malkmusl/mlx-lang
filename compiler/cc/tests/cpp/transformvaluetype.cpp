// The value type of a transformed range, std::ranges::range_value_t of
// the view (its iterator's traits: WebCore's CSSCalcExecutor.h, the NaN
// of an empty range's maximum).
#include <limits>
#include <ranges>
#include <stdio.h>
#include <type_traits>
#include <vector>

template<class R> auto maximum(R &&range)
{
    if (range.empty())
        return std::numeric_limits<std::ranges::range_value_t<R>>::max();
    auto it = range.begin();
    auto best = *it;
    for (++it; it != range.end(); ++it) {
        if (best < *it)
            best = *it;
    }
    return best;
}

int main(int argc, char **)
{
    std::vector<int> none;
    std::vector<int> some { 3, 9, 4 };
    auto half = [](int v) { return v / 2.0f; };
    auto viewed = some | std::views::transform(half);
    using Value = std::ranges::range_value_t<decltype(viewed)>;
    printf("%d %d\n", (int)std::is_same_v<Value, float>, (int)std::is_same_v<std::iterator_traits<decltype(viewed.begin())>::reference, float>);
    printf("%g %g\n", maximum(none | std::views::transform(half)), maximum(viewed));
    return argc;
}
