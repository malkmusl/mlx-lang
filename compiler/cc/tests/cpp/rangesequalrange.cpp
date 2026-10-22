// std::ranges::equal_range of a sorted array of pairs, with a comparison
// taking the key either side, as a std::span (PAL EncodingTables).
#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <utility>
struct CompareFirst {
    bool operator()(const std::pair<int, char>& a, int b) const { return a.first < b; }
    bool operator()(int a, const std::pair<int, char>& b) const { return a < b.first; }
    bool operator()(const std::pair<int, char>& a, const std::pair<int, char>& b) const { return a.first < b.first; }
    bool operator()(int a, int b) const { return a < b; }
};
static constexpr std::array<std::pair<int, char>, 6> table { { { 1, 0x61 }, { 3, 0x62 }, { 3, 0x63 }, { 5, 0x64 }, { 8, 0x65 }, { 9, 0x66 } } };
static std::span<const std::pair<int, char>> find(int key) { return std::ranges::equal_range(table, key, CompareFirst { }); }
int main()
{
    for (int key : { 0, 1, 3, 4, 9, 10 }) {
        auto range = find(key);
        printf("%d: %zu", key, range.size());
        for (auto& entry : range)
            printf(" %c", entry.second);
        printf("\n");
    }
    std::array<int, 7> numbers { 1, 2, 2, 2, 4, 6, 6 };
    auto twos = std::ranges::equal_range(numbers, 2);
    printf("2: at %d, %d of them, empty %d\n", int(twos.begin() - numbers.begin()), int(twos.size()), int(twos.empty()));
    auto fives = std::ranges::equal_range(numbers, 5);
    printf("5: at %d, empty %d\n", int(fives.begin() - numbers.begin()), int(fives.empty()));
    return 0;
}
