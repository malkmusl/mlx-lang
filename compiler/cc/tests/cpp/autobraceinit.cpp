// `auto x { e }` and `auto x(e)` take their type from e (C++17: a
// braced single element), for locals and statics (WebCore's `static
// constexpr auto dummyURLCharacters { "https://w/"_s }`).
#include <cstdio>
struct Lit { const char* s; unsigned n; };
constexpr Lit operator""_lit(const char* s, unsigned long n) { return Lit { s, (unsigned)n }; }
static auto number { 7 };
static constexpr auto letter { 'x' };
static auto parenthesized(2.5);
int main()
{
    auto local { 2.5f };
    auto lit { "local"_lit };
    static constexpr auto two { 2L };
    auto paren("paren");
    printf("%d %c %g %g %s %u %ld %s\n", number, letter, parenthesized, local, lit.s, lit.n, two, paren);
    return 1;
}
