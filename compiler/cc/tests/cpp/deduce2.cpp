// Probes of the WTF work (each its own namespace), compared with g++.
#include <stdio.h>
#include <utility>
#include <stddef.h>
#include <type_traits>
#include <new>
#include <concepts>

namespace ig1 {
template <class T> struct Raw { };
template <class T> struct Def { };
template <class T, class P = Raw<T>, class D = Def<T>> class R {
public:
    R(T &object) : p(&object) {}
    template <class X> R(X &o, int extra) : p(&o) { (void)extra; }
    R(const R &other) : p(other.p) {}
    T *p;
};
template <class X> struct W {};
template <class X> R(W<X> &) -> R<X>;
struct S { int v; };
template <class A, class B> struct Pair { A a; B b; Pair(A x, B y) : a(x), b(y) {} };
void run() { S s{5}; R r{s}; Pair pr(1, 2.5); printf("%d %d %g\n", r.p->v, pr.a, pr.b); }

}

namespace ic1 {
template <unsigned I> struct idx {};
struct valueless_t {};
template <typename... Ts> class B {
public:
  explicit constexpr B(valueless_t) : n(-1) {}
  template <unsigned I, typename... Args> explicit constexpr B(idx<I>, Args &&... args) : n(I + sizeof...(Args)) {}
  int n;
};
template <bool T, typename... Ts> class S;
template <typename... Ts> class S<true, Ts...> : public B<Ts...> { public: using B<Ts...>::B; };
template <typename... Ts> class S<false, Ts...> : public B<Ts...> { public: using B<Ts...>::B; ~S() {} };
template <typename... Ts> struct V { S<false, Ts...> impl_; V() : impl_(idx<0>{}) {} V(int x) : impl_(idx<1>{}, x, x) {} };
void run() { V<int, float> v; V<int, float> w(3); S<true, int> s(valueless_t{}); printf("%d %d %d\n", v.impl_.n, w.impl_.n, s.n); }

}

namespace ic2 {
struct A { int n; A(int x) : n(x) {} A(int x, int y) : n(x + y) {} template <class T> A(T *p) : n(100) {} };
struct D : A { using A::A; int extra = 7; };
void run() { D d(3); D e(3, 4); int k; D f(&k); printf("%d %d %d %d\n", d.n, e.n, f.n, d.extra); }

}

namespace ce1 {
constexpr size_t f() { size_t s = 1; s = 4 > s ? 4 : s; return s; }
template <typename... Ts> struct D {
  static constexpr size_t a = f();
  static constexpr size_t b = []{ return (size_t)4; }();
  static constexpr size_t c = []{ size_t s = 1; ((s = sizeof(Ts) > s ? sizeof(Ts) : s), ...); return s; }();
  alignas(b) unsigned char st[c];
};
void run() { D<char, double, int> d; printf("%zu %zu %zu %zu\n", D<char,double,int>::a, D<char,double,int>::b, D<char,double,int>::c, sizeof(d)); }

}

namespace da1 {
struct A { int x; int &get() { return x; } const int &cget() const { return x; } int val() { return x; } };
template <class V> constexpr decltype(auto) g(V &&v) { return std::forward<V>(v).get(); }
template <class V> constexpr decltype(auto) h(V &&v) { return std::forward<V>(v).val(); }
struct S { template <class V> static constexpr decltype(auto) k(V &&v) { return g(std::forward<V>(v)); } };
void run() { A a{3};
  printf("%d %d %d %d %d %d\n", std::is_reference<decltype((a.get()))>::value, std::is_reference<decltype(g(a))>::value, std::is_reference<decltype((g(a)))>::value, std::is_reference<decltype(h(a))>::value, std::is_reference<decltype((S::k(a)))>::value, std::is_reference<decltype((a.x))>::value); }

}

namespace dt1 {
struct Clock { using rep = long; static long now() { return 42; } struct inner { using t = short; }; };
template <class T> struct Has { template <class U> static std::true_type test(typename U::rep *); template <class U> static std::false_type test(...); static constexpr bool value = decltype(test<T>(0))::value; };
void run() { Clock c; decltype(c)::rep r = decltype(c)::now(); typename decltype(c)::inner::t s = 3; printf("%ld %zu %d %d\n", r, sizeof(s), Has<Clock>::value, Has<int>::value); }

}

namespace dt4 {
struct Clock { using rep = long; };
int g(long *); char g(...);
template <class U> int h(typename U::rep *);
template <class U> char h(...);
template <class U> int k(U *);
template <class U> char k(...);
void run() { printf("%zu %zu %zu %zu\n", sizeof(g(0)), sizeof(h<Clock>(0)), sizeof(h<int>(0)), sizeof(k<long>(0))); }

}

namespace us1 {
template <class... T> struct tl { static constexpr int n = sizeof...(T); };
namespace X { template <class... T> using V = tl<T...>; }
using X::V;
namespace Y { template <class A> struct b { using v = A; V<v, int> s; }; V<int, int, int> g; }
void run() { Y::b<int> bb; printf("%d %d\n", decltype(bb.s)::n, decltype(Y::g)::n); }

}

namespace es1 {
struct S { int v; template <typename C> static int make(int n); template <typename C> int get() const; };
template <> int S::make<char>(int n) { return n + 1; }
template <> int S::make<short>(int n) { return n + 2; }
template <> int S::get<char>() const { return v * 10; }
void run() { S s{4}; printf("%d %d %d\n", S::make<char>(1), S::make<short>(1), s.get<char>()); }

}

namespace ct1 {
struct Shape { unsigned len; const char *p; template <unsigned N> constexpr Shape(unsigned l, const char (&c)[N]); };
template <unsigned N> constexpr Shape::Shape(unsigned l, const char (&c)[N]) : len(l + N), p(c) { }
void run() { Shape s(1, "abc"); printf("%u %s\n", s.len, s.p); }

}

namespace ar1 {
template <class T> struct RP {}; template <class T> struct DT {};
template <class T, class P, class D> class R;
template <class T, class P = RP<T>, class D = DT<T>> R<T, P, D> adoptRef(T &);
template <class T, class P, class D> class R { public: T *p; R(T &o, int) : p(&o) {} friend R adoptRef<T, P, D>(T &); };
template <class T, class P, class D> inline R<T, P, D> adoptRef(T &r) { return R<T, P, D>(r, 0); }
struct S { int v; S(int a, int b) : v(a + b) {} };
void run() { alignas(S) char buf[sizeof(S)]; auto r = adoptRef(*new (buf) S { 1, 2 }); printf("%d\n", r.p->v); }

}

namespace ab1 {
template <class T> struct V { T x; bool has(const std::integral auto &v) const; template <class U> int f(U u) const; int g(const auto &a) const { return (int)sizeof(a); } };
template <class T> bool V<T>::has(const std::integral auto &v) const { return x == v; }
template <class T> template <class U> int V<T>::f(U u) const { return (int)u + x; }
void run() { V<int> v{3}; printf("%d %d %d %d\n", v.has(3), v.has(4L), v.f(2.5), v.g(1.0)); }

}

namespace ab3 {
template <class T, int N> struct V { T x; bool has(const auto &v) const; bool rl(const auto &v); template <class F> bool m(const F &f) { return f(x); } };
template <class T, int N> inline bool V<T, N>::has(const auto &v) const { return x == v; }
template <class T, int N> inline bool V<T, N>::rl(const auto &value) { return m([&value] (const T &c) { return c == value; }); }
void run() { V<int, 2> v{3}; printf("%d %d %d\n", v.has(3), v.has(4L), v.rl(3)); }

}

namespace ab4 {
template <class T, int N> struct V { T x; bool rl(const auto &v); void rl() { x = 0; } };
template <class T, int N> inline bool V<T, N>::rl(const auto &value) { return x == value; }
void run() { V<int, 2> v{3}; V<long, 1> w{4}; int a = v.rl(3); v.rl(); printf("%d %d %d\n", a, v.x, (int)w.rl(4)); }

}

namespace ab5 {
template <class T, int N> struct V { T x; template <class R> R map(const auto &f) const; };
template <class T, int N> template <class R> inline R V<T, N>::map(const auto &f) const { return (R)f(x); }
void run() { V<int, 2> v{3}; printf("%g\n", v.map<double>([](int a) { return a * 1.5; })); }

}

int main() {
    ig1::run();
    ic1::run();
    ic2::run();
    ce1::run();
    da1::run();
    dt1::run();
    dt4::run();
    us1::run();
    es1::run();
    ct1::run();
    ar1::run();
    ab1::run();
    ab3::run();
    ab4::run();
    ab5::run();
    return 0;
}
