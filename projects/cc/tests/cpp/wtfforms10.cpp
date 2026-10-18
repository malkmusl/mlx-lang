// WTF forms: parenthesized functional casts, global class arrays, instance member specializations, enum operators, nested lambda captures, auto & results, tuple_cat, empty packs, numeric_limits, converting optional, const references to converted values, braced temporaries in conditions, constrained parameters with requires, explicit arguments of the other kind, enums defined outside their class and in functions, conversions to explicitly given instances, not_fn, std::data of a list
#include <stdio.h>
#include <array>
#include <stdint.h>
#include <type_traits>
#include <utility>
#include <tuple>
#include <limits>
#include <optional>
#include <string>
#include <stddef.h>
#include <concepts>
#include <span>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <vector>

namespace p_pcast {
struct Mk { int m_value; int* p; explicit operator bool() const { return m_value != 0; }
  int f() { int r = 0; if ((bool(*this))) r += 1; if (!(bool(*this))) r += 10; if ((!!(!(bool(*p))))) r += 100; return r; } };
void run() { int x = 0; Mk m{5, &x}; Mk z{0, &x}; printf("%d %d\n", m.f(), z.f()); }
}

namespace p_ga1 {
struct CP { uint64_t s; int16_t b; int16_t d; };
static const std::array<CP, 2> kc { CP { 1, -2, -3 }, CP { 4, 5, 6 } };
const std::array<std::array<int, 3>, 2> fd { std::array<int, 3> { 0, 31, 59 }, std::array<int, 3> { 0, 31, 60 } };
void run() { printf("%d %d %d %d\n", (int)kc[1].s, kc[0].d, fd[0][2], fd[1][2]); }
}

namespace p_ms1 {
template <typename T> struct BF {
  static inline constexpr int minExp();
  static constexpr int other() { return sizeof(T); }
};
template <> inline constexpr int BF<double>::minExp() { return -22; }
template <> inline constexpr int BF<float>::minExp() { return -10; }
void run() { printf("%d %d %d\n", BF<double>::minExp(), BF<float>::minExp(), BF<float>::other()); }
}

namespace p_ff1 {
namespace ff {
enum class cf : uint64_t;
enum class cf : uint64_t { sci = 1, fixed = 4, general = fixed | sci, plus = 128 };
constexpr cf operator|(cf l, cf r) noexcept { using it = std::underlying_type<cf>::type; return static_cast<cf>(static_cast<it>(l) | static_cast<it>(r)); }
template <typename T> struct sup : std::integral_constant<bool, std::is_same<T, double>::value || std::is_same<T, float>::value> {};
template <typename UC> struct res { UC const* ptr; int ec; };
template <typename T, typename UC = char, typename = typename std::enable_if<(sup<T>::value), int>::type>
constexpr res<UC> from(UC const* first, UC const* last, T& value, cf fmt = cf::general) noexcept;
template <typename T, typename UC, typename>
constexpr res<UC> from(UC const* first, UC const* last, T& value, cf fmt) noexcept { value = (T)(last - first) + (T)(uint64_t)fmt; return { last, 0 }; }
}
void run() { double d = 0; const char* s = "hello"; auto r = ff::from(s, s + 5, d, ff::cf::general | ff::cf::plus); printf("%g %d\n", d, (int)(r.ptr - s)); }
}

namespace p_nl1 {
template <typename F> int call(F f) { return f(1); }
template <typename F> int call2(F f) { return f(2, 3); }
struct S { int k = 7; int go() { int base = 100; return call([&](int a) { return call2([&](int b, int c) { return a + b + c + base + k; }); }); } };
void run() {
  bool truncated = true; int count = 0;
  int r1 = call([truncated](int s) { return call2([truncated](int x, int y) -> int { return truncated ? x + y : 0; }) + s; });
  int r2 = call([&](int s) { return call2([&](int x, int y) { count += x; return count + y + s; }); });
  int r3 = call([=](int s) { return call2([=](int x, int y) { return truncated + x + y + s; }); });
  int r4 = call([&count](int s) { return call2([&count](int x, int) { return ++count + x; }); });
  printf("%d %d %d %d %d %d\n", r1, r2, r3, r4, count, S().go());
}
}

namespace p_fv1 {
template<typename T, int N> struct Vec { T data[N]; int sz = N; int capacity() const { return N; } void grow(int n) { sz = n; } };
template<typename C, int N, typename... A> auto& findVector(Vec<C, N>& buffer, A&&...) { return buffer; }
template<typename F, typename... A> auto& findVector(F&&, A&&... arguments) { return findVector(std::forward<A>(arguments)...); }
template<typename... A> int go(A&&... arguments) { auto& buffer = findVector(std::forward<A>(arguments)...); buffer.grow(buffer.capacity() + 1); return buffer.sz; }
void run() { Vec<char16_t, 32> v; const char16_t* p = u"x"; unsigned n = 1; printf("%d\n", go(p, n, v, nullptr)); }
}

namespace p_fv9 {
template<typename T> struct S { T x{}; T arr[3]{}; auto& get() { return x; } const auto& cget() const { return x; } auto* ptr() { return &x; } auto&& fwd() { return std::move(x); } auto&& lv() { return x; } };
struct P { int v = 4; auto& self() { return *this; } };
void run() {
  S<int> s; s.get() = 5; *s.ptr() += 1;
  static_assert(std::is_same_v<decltype(s.cget()), const int&>);
  static_assert(std::is_same_v<decltype(s.fwd()), int&&>);
  static_assert(std::is_same_v<decltype(s.lv()), int&>);
  static_assert(std::is_same_v<decltype(s.ptr()), int*>);
  P p; p.self().self().v = 9;
  printf("%d %d %d\n", s.cget(), s.x, p.v);
}
}

namespace p_tc1 {
int add3(const char* s, int a, double b) { printf("%s ", s); return a + (int)b; }
void run() {
  auto t = std::tuple_cat(std::make_tuple("x", 2), std::make_tuple(3.5));
  auto u = tuple_cat(std::make_tuple(1), std::tuple<>(), std::make_tuple(2, 3), std::make_tuple('c'));
  static_assert(std::tuple_size<decltype(u)>::value == 4);
  printf("%d %d %d %c ", std::get<0>(u), std::get<2>(u), (int)std::tuple_size<decltype(t)>::value, std::get<3>(u));
  printf("%d\n", std::apply(add3, t));
}
}

namespace p_tc9 {
template <class... Ts> struct tup;
template <> struct tup<> { static constexpr int n = 0; };
template <class T, class... R> struct tup<T, R...> { static constexpr int n = 1 + tup<R...>::n; };
template <class... B> struct X { typedef tup<int, B...> type; };
template <class... A> struct Y { typedef tup<A..., int> type; };
template <class... A> struct Z { typedef tup<int, A..., int> type; };
typedef X<>::type T1;
typedef Y<>::type T2;
typedef Z<>::type T3;
void run() { printf("%d %d %d\n", T1::n, T2::n, T3::n); }
}

namespace p_tc10 {
template <class... Ts> struct tup;
template <> struct tup<> { static constexpr int n = 0; };
template <class T, class... R> struct tup<T, R...> { static constexpr int n = 1 + tup<R...>::n; };
template <class T> struct P;
template <class... B> struct P<tup<B...> > { typedef tup<int, B...> type; };
template <class T, class U> struct Q;
template <class A, class... B> struct Q<tup<A>, tup<B...> > { typedef tup<A, B...> type; };
typedef P<tup<> >::type T1;
typedef Q<tup<int>, tup<> >::type T2;
void run() { printf("%d %d\n", T1::n, T2::n); }
}

namespace p_nl3 {
template<typename R> struct A { static bool f(R l) { return (std::numeric_limits<R>::max() - 1) < l; } };
void run() { printf("%d %lld %d %d %u %d %d %llu\n", A<long long>::f(5), std::numeric_limits<long long>::max(), (int)std::numeric_limits<signed char>::min(), (int)std::numeric_limits<char16_t>::max(), (unsigned)std::numeric_limits<char32_t>::max(), std::numeric_limits<int64_t>::is_signed, std::numeric_limits<wchar_t>::digits, std::numeric_limits<unsigned long long>::max()); }
}

namespace p_op1 {
struct Ch { unsigned long v; operator unsigned long() const { return v; } };
std::optional<unsigned long> f(bool b) { Ch c{42}; if (!b) return std::nullopt; return c; }
std::optional<std::string> g() { return "hi"; }
void run() { std::optional<long> o = 5; std::optional<double> d(3); printf("%lu %d %s %ld %g\n", *f(true), (int)f(false).has_value(), g()->c_str(), *o, *d); }
}

namespace p_ps1 {
struct FillWith {};
template<typename T> struct V { T a; size_t n; V(FillWith, size_t size, const T& val) : a(val), n(size) {} V(size_t size) : a(), n(size) {} };
struct Acc { V<long> chunk; int k; Acc() : chunk(FillWith { }, 3, 0LL), k { 1 } {} };
void run() { Acc x; V<long> v(FillWith{}, 2, 5LL); printf("%ld %zu %ld\n", x.chunk.a, x.chunk.n, v.a); }
}

namespace p_ps3 {
struct FillWith {};
template<typename T> struct V { T a; size_t n; V(FillWith, size_t size, const T& val) : a(val), n(size) {} V(size_t size) : a(), n(size) {} };
struct W { long a; W(FillWith, size_t size, const long& val) : a(val) {} };
void run() { V<long> v(FillWith{}, 2, 5L); W w(FillWith{}, 2, 5LL); V<long> v2(FillWith{}, 2, 5LL); printf("%ld %ld\n", v.a, w.a); }
}

namespace p_cd1 {
namespace ns { struct SV { const char* s; int len() const { int n = 0; while (s[n]) n++; return n; } bool has(char c) const { for (int i = 0; s[i]; i++) if (s[i] == c) return true; return false; } }; }
using ns::SV;
void run() { const char* p = "hello"; int r = 0;
  if (SV { p }.has('l')) r += 1;
  if (ns::SV { p }.len() == 5) r += 10;
  if (SV v { p }; v.len()) r += 100;

  printf("%d\n", r); }
}

namespace p_cn1 {
namespace W {
template<std::integral C1, std::integral C2> requires (sizeof(C1) == sizeof(C2))
inline size_t find(std::span<const C1> s, C2 m, size_t index = 0) { while (index < s.size()) { if (s[index] == m) return index; ++index; } return 99; }
template<typename C, typename F> requires (std::is_invocable_r_v<bool, F, C>)
inline size_t find(std::span<const C> s, F&& f, size_t start) { while (start < s.size()) { if (f(s[start])) return start; ++start; } return 98; }
struct Impl { std::span<const unsigned char> sp; template<typename F> requires (std::is_invocable_r_v<bool, F, char16_t>) size_t find(F f, size_t start) { return W::find(sp, f, start); } };
}
void run() { const unsigned char d[] = {1, 2, 3}; W::Impl i{std::span<const unsigned char>(d, 3)}; printf("%zu %zu\n", i.find([](char16_t c) { return c == 3; }, 0), W::find(std::span<const unsigned char>(d, 3), (unsigned char)2)); }
}

namespace p_ca1 {
enum class FA { Crash, Report };
template<typename T> struct Vec {
  T items[4]; int n = 0;
  template<typename... Args> void constructAndAppend(Args&&... args) { constructAndAppend<FA::Crash>(std::forward<Args>(args)...); }
  template<FA, typename... Args> bool constructAndAppend(Args&&...);
};
template<typename T> template<FA action, typename... Args> bool Vec<T>::constructAndAppend(Args&&... args) { items[n++] = T(std::forward<Args>(args)...); return action == FA::Crash; }
struct Str { const char* s; };
void run() { Vec<std::tuple<unsigned, unsigned, Str>> v; unsigned a = 1, b = 2; Str s{"x"}; v.constructAndAppend(a, b, s); printf("%d %u %s\n", v.n, std::get<1>(v.items[0]), std::get<2>(v.items[0]).s); }
}

namespace p_ca9 {
enum class FA { Crash, Report };
struct U {
  template<typename... Args> int h(Args&&... args) { return 0; }
  template<FA action, typename... Args> int h(Args&&... args) { return sizeof...(Args) * 10 + (int)action; } };
template<typename... Args> int k(Args&&... args) { return 0; }
template<FA action, typename... Args> int k(Args&&... args) { return sizeof...(Args) * 10 + (int)action; }
template<typename... Args> int w(Args&&... args) { return k<FA::Report>(args...); }
void run() { unsigned a = 1, b = 2; printf("%d %d %d\n", U().h<FA::Report>(a, b), k<FA::Report>(a, b), w(a, b)); }
}

namespace p_en1 {
struct P { enum class Part : uint8_t; enum class Kind; static int f(Part p); Kind k; };
enum class P::Part : uint8_t { A, B, C = 7 };
enum class P::Kind { X = 3, Y };
int P::f(Part p) { return (int)p + (int)Part::B; }
void run() { P p{P::Kind::Y}; printf("%d %d %d\n", P::f(P::Part::C), (int)p.k, (int)sizeof(P::Part)); }
}

namespace p_il1 {
template<typename C> struct It { std::span<const C> d; It() = default; It(std::span<const C> s) : d(s) {} bool atEnd() const { return d.empty(); } };
struct P { template<typename C> static bool at(It<C>); static bool loc(std::span<const unsigned char> s) { return at<unsigned char>(s); } };
template<typename C> bool P::at(It<C> it) { return !it.atEnd(); }
template<typename C> bool g(It<C> it) { return !it.atEnd(); }
void run() { unsigned char b[2] = {1, 2}; std::span<const unsigned char> s(b, 2); printf("%d %d\n", P::loc(s), g<unsigned char>(s)); }
}

namespace p_le1 {
struct P { template<typename C> int parse(C c); };
template<typename C> int P::parse(C c) {
  enum class State : uint8_t { Start, Mid, End };
  State s = State::Start;
  if (c > 0) s = State::End;
  return (int)s;
}
template<typename C> int f(C c) { enum class St { A, B }; St s = St::B; return (int)s + c; }
void run() { printf("%d %d %d\n", P().parse(1), P().parse(1.0), f(3)); }
}

namespace p_le3 {
int g(int c) { enum class St { A, B }; St s = St::B; return (int)s + c; }
int h(int c) { enum class St { A, B, C }; St s = St::C; return (int)s + c; }
void run() { printf("%d %d\n", g(3), h(3)); }
}

namespace p_nf1 {
template<typename C> bool isDig(C c) { return c >= '0' && c <= '9'; }
template<typename F> int count(const char* s, F f) { int n = 0; for (; *s; s++) if (f((char16_t)*s)) n++; return n; }
void run() { auto nd = std::not_fn(isDig<char16_t>); static_assert(std::is_invocable_r_v<bool, decltype(nd), char16_t>); printf("%d %d\n", count("a1b22", nd), count("a1b22", std::not_fn([](char16_t c) { return c == 'a'; }))); }
}

namespace p_il4 {
template<typename U> int sum(std::initializer_list<U> l) { auto s = std::span { std::data(l), l.size() }; int t = 0; for (auto x : s) t += x; return t + std::empty(l); }
void run() { std::vector<int> v{1, 2}; printf("%d %d %d\n", sum({1, 2, 3}), *std::data(v), (int)std::size(v)); }
}

int main() {
    p_pcast::run();
    p_ga1::run();
    p_ms1::run();
    p_ff1::run();
    p_nl1::run();
    p_fv1::run();
    p_fv9::run();
    p_tc1::run();
    p_tc9::run();
    p_tc10::run();
    p_nl3::run();
    p_op1::run();
    p_ps1::run();
    p_ps3::run();
    p_cd1::run();
    p_cn1::run();
    p_ca1::run();
    p_ca9::run();
    p_en1::run();
    p_il1::run();
    p_le1::run();
    p_le3::run();
    p_nf1::run();
    p_il4::run();
    return 0;
}
