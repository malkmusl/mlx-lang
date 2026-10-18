// Braced lists as arguments (`f({a, b})`, a class or a one-element
// scalar), class template argument deduction in expressions through
// the standard guides (`std::span { first, last }`, `std::pair(1,
// 2.5)`, `std::array { ... }`, `std::tuple`, `std::optional`),
// `decltype(auto)`, explicit template arguments an argument converts
// to, make_unsigned and is_integral of the character types and enums,
// static member templates defined outside their class beside an
// overload of the name, and value-initialized members (`T x { }`).
#include <stdio.h>
#include <stdint.h>
#include <span>
#include <vector>
#include <utility>
#include <string>
#include <array>
#include <tuple>
#include <optional>
#include <type_traits>
#include <concepts>

struct P { int x, y; };
static int sumP(P p) { return p.x + p.y; }
static int productRef(const P &p) { return p.x * p.y; }
static bool equal(const char *a, std::span<const char> b) { for (size_t i = 0; i < b.size(); ++i) if (a[i] != b[i]) return false; return true; }
static size_t count(const std::vector<int> &v) { return v.size(); }
static int first(std::pair<int, int> p) { return p.first; }
template <class T> T plus(T a, std::pair<T, T> b) { return a + b.first + b.second; }
static double scalar(double d) { return d; }
static size_t total(std::span<const unsigned short> s) { size_t t = 0; for (auto v : s) t += v; return t; }

struct V8 { uint8_t v[16]; };
constexpr V8 splat8(uint8_t c) { V8 r{}; for (auto &x : r.v) x = c; return r; }
template <typename Lane> decltype(auto) splat(Lane lane) { return splat8(static_cast<uint8_t>(lane)); }

template <typename T> concept IntegralOrEnum = std::integral<T> || std::is_enum_v<T>;
template <IntegralOrEnum T> constexpr auto unsignedCast(T value) { return static_cast<std::make_unsigned_t<T>>(value); }
enum class E : int { A = -1 };

struct H {
    template <typename T, unsigned N> static constexpr unsigned lit(const T (&chars)[N]);
    static constexpr unsigned lit(int x) { return x; }
};
template <typename T, unsigned characterCount>
constexpr unsigned H::lit(const T (&characters)[characterCount]) { return characterCount - 1 + characters[0]; }

template <size_t bits> class BitSet {
public:
    void set(size_t i) { words[i / 32] |= 1u << (i % 32); }
    bool get(size_t i) const { return words[i / 32] & (1u << (i % 32)); }
private:
    unsigned words[(bits + 31) / 32] { };
};
struct Partial { int a; int arr[3]; int b = 7; };

int main() {
    const char *s = "hello";
    printf("%d %d %d %zu %d %d %g %s\n", sumP({1, 2}), productRef({3, 4}), equal("hel", { s, 3 }), count({1, 2, 3}), first({7, 8}), plus(1, {2, 3}), scalar({2.5}), std::string({'a', 'b'}).c_str());
    const unsigned short data[4] = {1, 2, 3, 4};
    const unsigned short *p = data, *end = data + 4;
    auto sp = std::span { p, end };
    std::span whole(data);
    std::pair pr(1, 2.5);
    std::array arr { 1, 2, 3 };
    std::tuple tp(1, 'x', 2.0);
    std::optional op(7);
    std::vector<int> v { 4, 5 };
    std::span vs(v);
    printf("%zu %zu %zu %d %g %zu %c %d %zu %d\n", total(sp), whole.size(), total(std::span { p + 1, 2 }), pr.first, pr.second, arr.size(), std::get<1>(tp), *op, vs.size(), vs[1]);
    printf("%d %d\n", splat<uint8_t>('A').v[3], splat<char16_t>(u'B').v[0]);
    printf("%d %d %d\n", (int)unsignedCast(u'x'), (int)(unsignedCast(U'y') > 0), (int)(unsignedCast(E::A) > 0));
    printf("%u %u\n", H::lit("abc"), H::lit(5));
    BitSet<70> bitset; bitset.set(65);
    Partial part{1};
    printf("%d %d %d %d %d\n", bitset.get(65), bitset.get(3), part.arr[1], part.arr[2], part.b);
    return 0;
}
