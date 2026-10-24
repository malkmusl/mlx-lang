// `range | views::reverse` of a range given as a temporary (a std::span
// a function returns: WebCore's `list.usedValues() | std::views::reverse`)
// keeps it in the view; of a variable, it refers to it.
#include <ranges>
#include <span>
#include <stdio.h>
#include <vector>

struct Layers {
    std::vector<int> values { 1, 2, 3, 4, 5 };
    std::span<const int> used() const { return std::span<const int>(values.data(), 3); }
};

int main(int argc, char **)
{
    Layers layers;
    for (auto &value : layers.used() | std::views::reverse)
        printf("%d ", value);
    printf("| ");
    for (int value : std::views::reverse(layers.used()))
        printf("%d ", value);
    printf("| ");
    std::vector<int> &all = layers.values;
    for (int &value : all | std::views::reverse)
        value *= 10;
    for (int value : layers.values)
        printf("%d ", value);
    printf("\n");
    return argc;
}
