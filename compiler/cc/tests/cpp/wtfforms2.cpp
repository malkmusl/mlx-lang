// Forms from WTF's headers (2): builtins and overflow-free arithmetic, braced packs, constexpr string reads, by-value references, nested out-of-class classes, deferred default arguments, inherited types and enumerators, member templates after '.'.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <limits>
#include <limits.h>
#include <span>

namespace p_bi1 {
struct Shape { int len; Shape(int l) : len(l) {} };
struct Impl : private Shape { enum Tag { Make }; Impl(Tag, int n) : Shape(n) {} int get() const { return len; } };
struct Uniq : public Impl { protected: inline Uniq(Tag, int n); public: static Uniq *make(int n) { return new Uniq(Make, n); } };
inline Uniq::Uniq(Tag, int n) : Impl(Make, n) { }
void run() { printf("%d\n", Uniq::make(4)->get()); }
}

namespace p_bi2 {
struct Shape { int len; Shape(int l) : len(l) {} };
struct Impl : public Shape { enum Tag { Make }; Impl(Tag, int n) : Shape(n) {} int get() const { return len; } };
struct Uniq : public Impl { protected: inline Uniq(Tag, int n); public: static Uniq *make(int n) { return new Uniq(Make, n); } };
inline Uniq::Uniq(Tag, int n) : Impl(Make, n) { }
void run() { printf("%d\n", Uniq::make(4)->get()); }
}

namespace p_bi3 {
struct Shape { int len; Shape(int l) : len(l) {} };
struct Impl : private Shape { enum Tag { Make }; Impl(Tag, int n) : Shape(n) {} int get() const { return len; } };
struct Uniq : public Impl { protected: inline Uniq(Tag, int n) : Impl(Make, n) {} public: static Uniq *make(int n) { return new Uniq(Make, n); } };

void run() { printf("%d\n", Uniq::make(4)->get()); }
}

namespace p_bi4 {
struct Shape { int len; Shape(int l) : len(l) {} };
struct Impl : private Shape { enum Tag { Make }; Impl(Tag, int n) : Shape(n) {} int get() const { return len; } };
struct Uniq : public Impl { protected: inline Uniq(Tag, int n); public: static Uniq *make(int n) { return new Uniq(Make, n); } };
Uniq::Uniq(Tag, int n) : Impl(Make, n) { }
void run() { printf("%d\n", Uniq::make(4)->get()); }
}

namespace p_bi5 {
struct Impl { int len; Impl(int n) : len(n) {} };
struct Uniq : public Impl { Uniq(int n); };
Uniq::Uniq(int n) : Impl(n) { }
struct B2 { enum Tag { Make }; int len; B2(Tag, int n) : len(n) {} };
struct U2 : B2 { U2(Tag, int n); };
U2::U2(Tag, int n) : B2(Make, n) { }
void run() { Uniq u(4); U2 v(B2::Make, 5); printf("%d %d\n", u.len, v.len); }
}

namespace p_bt1 {
struct P { int v; void dump() const { printf("%d\n", v); } };
template <class T> struct Q { T v; void dump() const { printf("q%d\n", (int)v); } };
void run() { P { 3 }.dump(); Q<long> { 4 }.dump(); P p { 5 }; p.dump(); return; }
}

namespace p_cs1 {
template <size_t N> constexpr uintptr_t hashOf(const char (&str)[N]) { uintptr_t r = 134775813; for (size_t i = 0; i < N; ++i) r += ((r * str[i]) ^ (r >> 16)); return r & 0xffff; }
enum class Tag : uintptr_t { None = 0, C = 1 };
constexpr Tag t1 = static_cast<Tag>(hashOf("JITThunk"));
static_assert(t1 != Tag::None && t1 != Tag::C);
constexpr size_t len(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
static_assert(len("hello") == 5);
void run() { printf("%lu %zu\n", (unsigned long)t1, len("abc")); }
}

namespace p_cx1 {
template <class C> bool ok(unsigned long n) { constexpr unsigned long mx = std::min(static_cast<unsigned long>(100), (std::numeric_limits<unsigned>::max() - 8) / sizeof(C)); return n <= mx; }
void run() { constexpr int k = 3 * 4; printf("%d %d %d\n", ok<char>(50), ok<int>(500), k); }
}

namespace p_en1 {
struct Sh { enum Tag { Make = 7 }; int v; constexpr Sh(int a, Tag t) : v(a + t) {} };
struct Outer : Sh { struct Inner : private Sh { constexpr Inner(int a); int get() const { return v; } }; Outer() : Sh(1, Make) {} };
constexpr Outer::Inner::Inner(int a) : Sh(a, Make) {}
void run() { Outer::Inner i(3); Outer o; printf("%d %d\n", i.get(), o.v); }
}

namespace p_nc1 {
struct O { struct I { int v; template <unsigned N> constexpr I(const char (&c)[N]); template <class T> int f(T t) const; }; };
template <unsigned N> constexpr O::I::I(const char (&c)[N]) : v(N) {}
template <class T> int O::I::f(T t) const { return v + (int)t; }
void run() { O::I i("abcd"); printf("%d %d\n", i.v, i.f(2.0)); }
}

namespace p_nt1 {
struct SV {
  int v;
  template <unsigned N> class Up { public: Up(const SV &s) : n(s.v + N) {} int n; };
  template <unsigned N = 3> Up<N> up() const;
};
template <unsigned N> inline SV::Up<N> SV::up() const { return Up<N>(*this); }
void run() { SV s{4}; printf("%d %d\n", s.up().n, s.up<10>().n); }
}

namespace p_nt2 {
struct SV {
  int v;
  template <unsigned N> class Up;
  using Up32 = Up<32>;
  struct In;
  template <unsigned N = 3> Up<N> up() const;
};
template <unsigned N> class SV::Up { public: explicit Up(const SV &s); int n; };
struct SV::In { int k; int twice() const { return k * 2; } };
template <unsigned N> inline SV::Up<N> SV::up() const { return Up<N>(*this); }
template <unsigned N> inline SV::Up<N>::Up(const SV &s) : n(s.v + N) {}
void run() { SV s{4}; SV::In i{5}; SV::Up32 u(s); printf("%d %d %d %d\n", s.up().n, s.up<10>().n, i.twice(), u.n); }
}

namespace p_ov2 {
void run() {
  unsigned long r; int i; long l; unsigned u;
  int a = __builtin_mul_overflow(0xffffffffffffffffUL, 2UL, &r);
  int b = __builtin_add_overflow(INT_MAX, 1, &i);
  int c = __builtin_sub_overflow(0u, 1u, &u);
  int d = __builtin_add_overflow(-5, 3, &i);
  int e = __builtin_mul_overflow(LONG_MIN, -1L, &l);
  int f = __builtin_add_overflow(40, 2, &l);
  printf("%d %lu %d %d %u %d %d %d %ld\n", a, r, b, c, u, d, i, e, l);
  printf("%d %d\n", f, __has_builtin(__builtin_mul_overflow));
  return;
}
}

namespace p_qm {
struct B { int h; }; struct S : B { char c; double d; static size_t f() { return sizeof(S::d) + sizeof(S::h); } };
void run() { printf("%zu %zu %zu %zu\n", sizeof(S::c), sizeof(S::d), S::f(), offsetof(S, d)); }
}

namespace p_sh1 {
struct Conv {};
struct H { template <typename T, typename C = Conv> static unsigned hash(std::span<const T> data); };
struct V { const char *p; unsigned n; std::span<const char> s8() const { return { p, n }; } unsigned h() const { return H::hash(s8()); } };
template <typename T, typename C> unsigned H::hash(std::span<const T> data) { return (unsigned)data.size() * 10; }
void run() { V v{"abc", 3}; printf("%u\n", v.h()); }
}

namespace p_sh2 {
struct Conv {};
struct H { template <typename T, typename C = Conv> static unsigned hash(std::span<const T> data); };
struct V { const unsigned char *p; unsigned n; std::span<const unsigned char> s8() const { return { p, n }; } unsigned h() const { return H::hash(s8()); } };
template <typename T, typename C> unsigned H::hash(std::span<const T> data) { return (unsigned)data.size() * 10; }
void run() { static const unsigned char d[3] = {1,2,3}; V v{d, 3}; printf("%u\n", v.h()); }
}

namespace p_sp1 {
struct Lit { const char *p; unsigned n; std::span<const char> s() const { return { p, n }; } };
template <typename T> class Ad;
template <typename C> class Ad<std::span<const C>> { public: Ad(std::span<const C> c) : m(c) {} unsigned len() const { return m.size(); } private: std::span<const C> m; };
template <> class Ad<Lit> : public Ad<std::span<const char>> { public: Ad(Lit l) : Ad<std::span<const char>> { l.s() } { } };
void run() { Ad<Lit> a(Lit{"abcd", 4}); printf("%u\n", a.len()); }
}

namespace p_sp2 {
struct B { int v; B(int x) : v(x) {} };
template <typename T> class Ad;
template <> class Ad<int> : public B { public: Ad(int l) : B(l) { } };
template <> class Ad<char> : public B { public: Ad(char l) : B { l } { } };
template <typename T> struct W { int v; W(int x) : v(x) {} };
template <> class Ad<long> : public W<long> { public: Ad(long l) : W<long> { (int)l } { } };
void run() { Ad<int> a(3); Ad<char> b(4); Ad<long> c(5L); printf("%d %d %d\n", a.v, b.v, c.v); }
}

namespace p_tr1 {
struct SV { int v;
  template <typename C> int trim(int x, int (*p)(int)) const { return (int)sizeof(C) * 100 + p(x) + v; }
  int trim(int (*p)(int)) const { return v > 0 ? trim<char>(v, p) : trim<long>(v, p); }
  static int sq(int x) { return x * x; } };
void run() { SV a{3}, b{-2}; printf("%d %d\n", a.trim(SV::sq), b.trim(SV::sq)); }
}

int main() {
    p_bi1::run();
    p_bi2::run();
    p_bi3::run();
    p_bi4::run();
    p_bi5::run();
    p_bt1::run();
    p_cs1::run();
    p_cx1::run();
    p_en1::run();
    p_nc1::run();
    p_nt1::run();
    p_nt2::run();
    p_ov2::run();
    p_qm::run();
    p_sh1::run();
    p_sh2::run();
    p_sp1::run();
    p_sp2::run();
    p_tr1::run();
    return 0;
}
