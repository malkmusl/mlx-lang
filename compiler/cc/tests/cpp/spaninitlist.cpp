// `std::span { list }` of a std::initializer_list parameter: a span of
// its const elements (WebCore's FontCache, the families whose fonts show
// a backslash as a yen sign, by their names given as UTF-16 lists).
#include <cstdio>
#include <initializer_list>
#include <span>
#include <type_traits>

static unsigned sum(std::span<const char16_t> units)
{
    unsigned total = 0;
    for (auto unit : units)
        total += unit;
    return total;
}

int main()
{
    auto add = [&](const char* name, std::initializer_list<char16_t> unicodeName) {
        auto units = std::span { unicodeName };
        static_assert(std::is_same_v<decltype(units), std::span<const char16_t>>);
        printf("%s: %zu units, first %x, sum %u\n", name, units.size(), units[0], sum(units));
    };
    add("MS Gothic", { 0xFF2D, 0xFF33, 0x0020, 0x30B4, 0x30B7, 0x30C3, 0x30AF });
    add("Meiryo", { 0x30E1, 0x30A4, 0x30EA, 0x30AA });
    std::initializer_list<int> numbers { 3, 4, 5 };
    std::span view(numbers);
    printf("numbers: %zu, last %d\n", view.size(), view.back());
    // (Two pointers in braces: the span between them.)
    const unsigned char bytes[4] = { 1, 2, 3, 4 };
    const unsigned char* pointer = bytes;
    auto* end = pointer + 4;
    auto between = std::span { pointer, end };
    static_assert(std::is_same_v<decltype(between), std::span<const unsigned char>>);
    printf("between: %zu\n", between.size());
    return 0;
}
