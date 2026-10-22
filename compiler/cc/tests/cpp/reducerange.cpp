// std::reduce of a range alone (the sum from a value-initialized
// element) and with a binary operation (GridLayoutUtils's sum of track
// sizes).
#include <stdio.h>
#include <numeric>
#include <functional>
#include <span>
struct Length {
    int value = 0;
    Length operator+(Length other) const { return { value + other.value }; }
};
int main(int argc, char** argv)
{
    Length sizes[] = { { 3 }, { 4 }, { 5 } };
    std::span<const Length> all { sizes };
    auto before = all.subspan(0, 2);
    auto sum = std::reduce(before.begin(), before.end());
    int factors[] = { 2, 3, 7 };
    auto product = std::reduce(factors, factors + 3, 1, std::multiplies<int>());
    printf("%d %d\n", sum.value, product);
    return argc - 1;
}
