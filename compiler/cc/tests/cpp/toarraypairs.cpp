// std::to_array of elements without a default constructor (WTF's
// SortedArrayMap of `std::pair<ComparableLettersLiteral, ASCIILiteral>`
// in ResourceLoaderSoup.cpp): the array made from the elements, not
// default-made and assigned ("no matching constructor").
#include <stdio.h>
#include <array>
#include <utility>
#include <type_traits>

struct Lit {
    const char* s;
    constexpr Lit(const char* p) : s(p) { }
};
inline constexpr Lit operator""_s(const char* p, size_t) { return Lit(p); }
enum class Subset { All, NoUpper };
template<Subset> struct Comparable {
    Lit literal;
    constexpr Comparable(Lit);
};
template<Subset subset> constexpr Comparable<subset>::Comparable(Lit l) : literal(l) { }
using Letters = Comparable<Subset::NoUpper>;

template<typename T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> toArray(T (&&array)[N])
{
    return std::to_array<T>(std::move(array));
}
template<typename T> struct Map {
    T storage;
    constexpr Map(const T& a) : storage(a) { }
};

int main(int argc, char **argv)
{
    static constexpr Map mappings { toArray<std::pair<Letters, Lit>>({
        { ".css"_s, "text/css"_s },
        { ".gif"_s, "image/gif"_s },
    }) };
    printf("%s %s\n", mappings.storage[1].first.literal.s, mappings.storage[0].second.s);
    int plain[3] = { 1, 2, 3 };
    auto copied = std::to_array(plain);
    auto pairs = std::to_array<std::pair<int, long>>({ { 4, 5L }, { 6, 7L } });
    printf("%d %d %ld\n", copied[2], pairs[1].first, pairs[0].second);
    return argc;
}
