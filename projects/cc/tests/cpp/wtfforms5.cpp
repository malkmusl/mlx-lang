// Forms from WTF's sources (5): lambdas as function pointers, constructor templates in implicit conversions, a member template's own parameters before the class's members, arrays and initializer_lists from braces, to_array, more of <algorithm> and std::ranges, static locals in braces, std::exchange with braces, a base's members by its typedef, const and non-const member template overloads, the address of a constexpr variable, dependent-typed non-type parameters, declared defaults, x.f<T>() beside f.
#include <stdio.h>
#include <stddef.h>
#include <span>
#include <array>
#include <type_traits>
#include <algorithm>
#include <vector>
#include <utility>
#include <string>
#include <initializer_list>
#include <limits>

namespace p_lf1 {
static int apply(int x, int (*f)(int)) { return f(x); }
static bool any(const char* s, bool (*pred)(char16_t)) { for (; *s; s++) if (pred(*s)) return true; return false; }
void run() {
    auto twice = [](int v) { return v * 2; };
    int (*fp)(int) = twice;
    auto isQ = [] (char16_t c) { return c == '?' || c == '#'; };
    void (*hello)() = [] { printf("hi\n"); };
    hello();
    printf("%d %d %d %d %d\n", apply(3, twice), fp(5), apply(4, [](int v) { return v + 1; }), any("ab?", isQ), any("ab", isQ));
}
}

namespace p_lf2 {
template<typename S> int count(const S& s, bool (*pred)(char)) { int n = 0; for (const char* p = s; *p; p++) n += pred(*p); return n; }
void run() { const char* t = "a?b#"; auto q = [] (char c) { return c == '?' || c == '#'; }; printf("%d\n", count(t, q)); }
}

namespace p_ar1 {
template<typename T> constexpr size_t ms(T* p, size_t n) { return n + sizeof(T); }
struct Lit {
    size_t n;
    template<size_t length> consteval Lit(const char (&literal)[length]) : n(ms(literal, length)) {}
};
struct Imp { Lit l; Imp(Lit x) : l(x) {} };
static Imp blank { "about:blank" };
void run() { const char a[4] = "abc"; printf("%zu %zu\n", ms(a, 4), blank.l.n); }
}

namespace p_ar3 {
template<typename T, std::size_t Extent = std::dynamic_extent>
inline constexpr auto ums(T* ptr, size_t size) { return std::span<T, Extent> { ptr, size }; }
struct Lit {
    std::span<const char> s;
    Lit() = default;
    template<size_t length> consteval Lit(const char (&literal)[length]) : s(ums(literal, length)) {}
    size_t n() const { return s.size(); }
};
enum Kind { Normal, Other };
struct Imp { Lit l; explicit constexpr Imp(Lit x, Kind k = Normal) : l(x) {} };
static Imp blank { "about:blank" };
void run() { printf("%zu\n", blank.l.n()); }
}

namespace p_tp1 {
struct Lit {
    size_t n;
    template<size_t length> constexpr Lit(const char (&literal)[length]) : n(length + 100) {}
    template<int value> int add() const { return value + (int)n; }
    size_t length() const { return n; }
    int value() const { return 1; }
};
void run() { Lit l("abc"); printf("%zu %d\n", l.length(), l.add<5>()); }
}

namespace p_ta2 {
template<typename T, size_t N> constexpr std::array<std::remove_cv_t<T>, N> toArray(T (&&a)[N]) { std::array<std::remove_cv_t<T>, N> r { }; for (size_t i = 0; i < N; i++) r[i] = a[i]; return r; }
void run() { auto l = toArray<unsigned short>({ 0, 1, 7, 9 }); auto m = toArray({ 5 }); printf("%zu %d %zu %d\n", l.size(), l[2], m.size(), m[0]); }
}

namespace p_ta3 {
template<typename T, size_t N> int sum(T (&&a)[N]) { int s = 0; for (size_t i = 0; i < N; i++) s += a[i]; return s; }
int f(const int (&a)[3]) { return a[0] + a[1] + a[2]; }
void run() { printf("%d %d %d\n", sum<int>({1, 2, 3}), sum({4, 5}), f({7, 8, 9})); }
}

namespace p_ta4 {
void run() { int a[3] = {1, 2, 3}; auto x = std::to_array(a); auto y = std::to_array<long>({4, 5}); printf("%zu %d %zu %ld\n", x.size(), x[2], y.size(), y[1]); }
}

namespace p_al1 {
void run() {
    static constexpr std::array<unsigned short, 6> ports { 0, 1, 7, 9, 21, 80 };
    int v[] = { 5, 3, 9, 1, 7 };
    std::sort(std::begin(v), std::end(v));
    std::vector<int> w { 4, 2, 8 };
    std::ranges::sort(w);

    printf("%d %d %d %d\n", std::is_sorted(std::begin(ports), std::end(ports)), std::binary_search(std::begin(ports), std::end(ports), (unsigned short)21), std::binary_search(ports.begin(), ports.end(), (unsigned short)22), (int)(std::lower_bound(v, v + 5, 6) - v));
    printf("%d %d %d %d %d %d\n", *std::max_element(v, v + 5), std::max({3, 8, 2}), w[0], 0, std::clamp(15, 0, 10), (int)std::ranges::count(w, 8));
    printf("%d %d\n", std::all_of(v, v + 5, [](int x) { return x > 0; }), std::ranges::any_of(w, [](int x) { return x == 2; }));
}
}

namespace p_al3 {
std::array<unsigned short, 3> g { 0, 1, 7 };
struct W { int e[3]; };
W w { 4, 5, 6 };
void run() { static W sw { 1, 2, 3 }; printf("%d %d %d\n", g[2], w.e[1], sw.e[2]); }
}

namespace p_ex1 {
void run() { std::string s = "abc"; std::string old = std::exchange(s, { }); int i = 5; int j = std::exchange(i, 7); printf("[%s] [%s] %d %d\n", old.c_str(), s.c_str(), i, j); }
}

namespace p_ex2 {
template<typename T> struct Set {
    Set() {}
    Set(std::initializer_list<T>) {}
    template<typename C> explicit Set(C&& c) { if (!c.size()) return; }
    bool has(const T&) const { return true; }
};
template<typename F> struct Fn { template<typename L> Fn(L&& l) : f(1) {} int f; };
int rq(int, const Set<int>&) { return 1; }
int rq(int, const Fn<bool(int)>&) { return 2; }
void run() { Set<int> keys; printf("%d\n", rq(1, [&](auto& k) { return keys.has(k); })); }
}

namespace p_bs1 {
namespace d { template<class T> struct base { T s; int idx() const { return 7; } }; }
template<class T> class ex : private d::base<T> {
    typedef d::base<T> base;
public:
    ex(T v) { base::s = v; }
    bool has() const { return base::s != 0; }
    T get() const { return base::s + base::idx(); }
};
void run() { ex<int> e(3); printf("%d %d\n", e.has(), e.get()); }
}

namespace p_bs4 {
template<class T> struct base { T s; };
template<class T> class ex : private base<T> {
    typedef base<T> B;
public:
    ex(T v) { B::s = v; }
    T one() const { return B::s; }
    T two() const { return B::s; }
};
void run() { ex<int> e(3); printf("%d %d\n", e.one(), e.two()); }
}

namespace p_cf1 {
struct It { int v; };
struct CIt { int v; };
struct Table {
    template<typename H, bool b, typename T> It find(const T&);
    template<typename H, bool b, typename T> CIt find(const T&) const;
};
template<typename H, bool b, typename T> It Table::find(const T& x) { return It { (int)x + 1 }; }
template<typename H, bool b, typename T> CIt Table::find(const T& x) const { return CIt { (int)x + 2 }; }
struct Adapter { Adapter(const It& i) : v(i.v) {} int v; };
template<typename K> struct Map {
    Table m_impl;
    template<typename H, typename T> Adapter find(const T& v) { return m_impl.template find<H, true>(v); }
};
void run() { Map<int> m; const Table t{}; printf("%d %d\n", m.find<int>(5).v, t.find<int, true>(5).v); }
}

namespace p_cf2 {
struct Table {
    template<typename T> int find(const T&);
    template<typename T> int find(const T&) const;
};
template<typename T> int Table::find(const T& x) { return 1; }
template<typename T> int Table::find(const T& x) const { return 2; }
void run() { Table m; const Table t{}; printf("%d %d\n", m.find(5), t.find(5)); }
}

namespace p_ce1 {
static const char* nz() { static constexpr char empty = 'Q'; return &empty; }
constexpr int N = 0x1234;
void run() { const int* p = &N; printf("%c %x %d\n", *nz(), *p, nz() == nz()); }
}

namespace p_nt3 {
template<typename E, typename std::underlying_type<E>::type constant = std::numeric_limits<typename std::underlying_type<E>::type>::max()>
struct EMT { static constexpr E empty() { return static_cast<E>(constant); } };
enum class Ty : unsigned char { A, B };
void run() { printf("%d %d\n", (int)EMT<Ty>::empty(), (int)EMT<Ty, 7>::empty()); }
}

namespace p_pi1 {
template<typename I> I parse(const char*, int base = 10, bool ws = true);
template<typename I> I parse(const char* s, int base, bool ws) { return (I)(base + (ws ? 1 : 0) + s[0]); }
void run() { printf("%d %d\n", parse<int>("\x01"), parse<int>("\x01", 16, false)); }
}

namespace p_mf1 {
struct Tr { static int h(int v) { return v * 10; } };
template<typename K> struct Map {
    int find(const K& k) { return k; }
    template<typename H> int find(const auto& v) { return H::h(v); }
};
void run() { Map<int> m; auto* p = &m; printf("%d %d %d\n", p->find(3), p->find<Tr>(4), m.find<Tr>(5)); }
}

int main() {
    p_lf1::run();
    p_lf2::run();
    p_ar1::run();
    p_ar3::run();
    p_tp1::run();
    p_ta2::run();
    p_ta3::run();
    p_ta4::run();
    p_al1::run();
    p_al3::run();
    p_ex1::run();
    p_ex2::run();
    p_bs1::run();
    p_bs4::run();
    p_cf1::run();
    p_cf2::run();
    p_ce1::run();
    p_nt3::run();
    p_pi1::run();
    p_mf1::run();
    return 0;
}
