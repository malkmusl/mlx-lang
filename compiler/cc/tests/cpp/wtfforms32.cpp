// WebKit JSC forms: CTAD through member aliases, ranges::lower_bound with a projection, float limits' exponents, a span deduced in an initializer, range-for over a braced list, static arrays of unknown bound
#include <stdio.h>
#include <span>
#include <limits>
#include <vector>
#include <algorithm>
#include <stdint.h>
#include <initializer_list>
#include <array>
#include <utility>
#include <iterator>

namespace p_ct4 {
namespace W {
template <typename T> class Buf final {
public:
    using C = T;
    constexpr Buf() = default;
    constexpr Buf(std::span<const C> c) : m_data(c) { }
    size_t size() const { return m_data.size(); }
    C first() const { return m_data[0]; }
private:
    std::span<const C> m_data;
};
struct SV { std::span<const char> span8() const { return std::span<const char>("hey", 3); } bool is8() const { return true; } };
template <typename S, typename F> decltype(auto) readFor(S&& s, const F& f)
{
    if (s.is8())
        return f(Buf { s.span8() });
    return f(Buf { s.span8() });
}
}
using W::Buf;
namespace J {
template <typename C> static int parse(Buf<C>& b) { return (int)b.size() * 100 + b.first(); }
int parseIt(W::SV s) { return readFor(s, [](auto buffer) -> int { return parse(buffer); }); }
}
int run() { printf("%d\n", J::parseIt(W::SV { })); return 0; }
}

namespace p_rl1 {
struct Entry { const char* name; int id; };
static bool lessName(const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return (unsigned char)*a < (unsigned char)*b; }
int run() {
    std::vector<Entry> v { { "alpha", 1 }, { "beta", 2 }, { "delta", 3 }, { "gamma", 4 } };
    auto it = std::ranges::lower_bound(v.begin(), v.end(), "delta", lessName, &Entry::name);
    auto it2 = std::ranges::lower_bound(v, "c", lessName, &Entry::name);
    static constexpr int bits = std::numeric_limits<uint64_t>::digits - std::numeric_limits<double>::digits;
    printf("%d %d %d %d %d %d\n", it->id, it2->id, std::numeric_limits<double>::max_exponent, bits, std::numeric_limits<float>::min_exponent10, std::numeric_limits<double>::max_exponent10);
    return 0;
}
}

namespace p_sp1 {
struct Str { size_t n; Str(std::span<const char16_t> s) : n(s.size()) { } };
int run() {
    const char16_t* p = u"hello";
    int len = 4;
    Str s(std::span(p, len));
    int sum = 0;
    for (auto c : { u'y', u'M', u'd' }) sum += c;
    printf("%zu %d\n", s.n, sum);
    return 0;
}
}

namespace p_ar1 {
static unsigned f() {
    static constexpr std::pair<std::array<const char, 3>, unsigned> units[] = {
        { { 'B', 'H', 'D' }, 3 },
        { { 'X', 'P', 'F' }, 0 },
        { { 'Y', 'P', 'F' }, 1 },
    };
    return std::size(units) * 10 + units[0].second + units[2].first[0];
}
int run() { printf("%u\n", f()); return 0; }
}

int main() {
    p_ct4::run();
    p_rl1::run();
    p_sp1::run();
    p_ar1::run();
    return 0;
}
