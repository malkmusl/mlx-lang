// std::pair's three-way comparison (C++20), first by first.
#include <compare>
#include <cstdio>
#include <utility>

struct Highlight {
    unsigned start { 0 };
    unsigned end { 0 };
    friend std::strong_ordering operator<=>(const Highlight& a, const Highlight& b)
    {
        return std::make_pair(a.start, a.end) <=> std::make_pair(b.start, b.end);
    }
    friend bool operator==(const Highlight&, const Highlight&) = default;
};

int main(int argc, char** argv)
{
    Highlight a { 1, 5 };
    Highlight b { 1, 7 };
    Highlight c { (unsigned)argc, 5 };
    std::printf("%d %d %d %d\n", (int)(a < b), (int)(b < a), (int)(a <= c), (int)(c > b));
    auto p = std::make_pair(2, 3) <=> std::make_pair(2, 3);
    std::printf("%d %d\n", (int)(p == 0), (int)(std::make_pair(1, 9) < std::make_pair(2, 0)));
    return 0;
}
