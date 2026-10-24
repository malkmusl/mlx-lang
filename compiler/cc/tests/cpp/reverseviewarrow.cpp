// `->` on the iterator of views::reverse (WebCore's GraphicsContextSkia:
// the innermost layer of its state stack found from the top).
#include <algorithm>
#include <cstdio>
#include <optional>
#include <ranges>
#include <vector>

struct State {
    bool isLayer;
    int alpha;
    std::optional<int> mode;
};

int main()
{
    std::vector<State> stack { { false, 1, std::nullopt }, { true, 2, 5 }, { false, 3, std::nullopt }, { true, 4, std::nullopt } };
    auto reversed = stack | std::views::reverse;
    auto it = std::ranges::find(reversed, true, &State::isLayer);
    if (it != reversed.end())
        printf("top layer: alpha %d, mode %s\n", it->alpha, it->mode ? "yes" : "no");
    do
        ++it;
    while (it != reversed.end() && !it->isLayer);
    if (it != reversed.end() && it->mode)
        printf("next layer: alpha %d, mode %d\n", it->alpha, *it->mode);
    it->alpha = 20;
    printf("written: %d\n", stack[1].alpha);
    return 0;
}
