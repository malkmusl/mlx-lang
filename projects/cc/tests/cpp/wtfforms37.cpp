// WTF/JSC forms: const references to converted pointers, class results completed at calls, braced lists and reference parameters, inner pack expansions in folds, variadic tie-breaks, bidirectional bound searches, compound assignment in requires, constexpr switch label groups
#include <stdio.h>
#include <span>
#include <cstddef>
#include <utility>
#include <set>
#include <vector>
#include <algorithm>
#include <iterator>
#include <stdint.h>

namespace p_ev1 {
struct Base { int x; };
struct Derived : Base {};
template<typename K, typename V> struct Map {
    using KeyType = K;
    template<typename F> V* ensureValue(const KeyType& key, F&& f) { V* v = f(); return v; }
};
struct Id { Derived* p; Derived* impl() const { return p; } };
int run() {
    Map<Base*, int> m; Derived d; d.x = 3; Id id{&d}; int z = 7;
    int* r = m.ensureValue(id.impl(), [&] { return &z; });
    printf("%d\n", *r);
    return 0;
}
}

namespace p_ev3 {
struct Base { int x; };
struct Derived : Base {};
using K = Base*;
int f(const K& k) { return k->x; }
int f(int a, int b) { return 0; }
template<typename F> int g(F&& fn) { return fn(); }
template<typename F> int h(const K& k) { return k->x; }
Derived* get(Derived* d) { return d; }
int run() {
    Derived d; d.x = 3;
    printf("%d\n", f(get(&d)));
    printf("%d\n", g([] { return 1; }));
    printf("%d\n", h<int>(get(&d)));
    return 0;
}
}

namespace p_rc1 {
struct Impl { int n = 1; void ref() { n++; } void deref() { n--; } };
struct Reg : Impl { int k = 5; };
template<typename T> struct Ref {
    T* p;
    Ref(T& r) : p(&r) { p->ref(); }
    Ref(const Ref& o) : p(o.p) { p->ref(); }
    ~Ref() { if (p) p->deref(); }
    operator T&() const { return *p; }
};
struct Registry { Ref<Reg> symbolForKey(int); };
struct Sym { static int create(int vm, Impl& uid); };
long encode(int v) { return v + 1000; }
long f(Registry& r, int vm) { return encode(Sym::create(vm, r.symbolForKey(3))); }
Reg g;
Ref<Reg> Registry::symbolForKey(int k) { g.k = k; return Ref<Reg>(g); }
int Sym::create(int vm, Impl& uid) { return uid.n * 10 + vm; }
int run() { Registry r; long v = f(r, 2); printf("%ld %d %d\n", v, g.n, g.k); return 0; }
}

namespace p_bl1 {
struct Impl { int n; Impl(int a, int b) : n(a + b) {} };
struct S {
    int v;
    S(Impl& i) : v(i.n) {}
    S(Impl* i) : v(-1) {}
    S(std::span<const char16_t> s) : v((int)s.size()) {}
};
int run() {
    const char16_t* p = u"hello";
    S a({ p, 4 });
    Impl i(1, 2);
    S b(i);
    printf("%d %d\n", a.v, b.v);
    return 0;
}
}

namespace p_bl2 {
enum class Tag { X };
struct Impl { int n; Impl(std::span<const char16_t> s, Tag) : n(100) {} Impl(unsigned length) : n(length) {} };
struct SV {
    int v;
    SV(const Impl& i) : v(i.n) {}
    SV(std::span<const char> s) : v(-2) {}
    SV(std::span<const char16_t> s) : v((int)s.size()) {}
};
struct P { int a; P(int* p, int n) : a(n) {} };
struct Q { int v; Q(const P& p) : v(p.a) {} Q(double) : v(-1) {} };
int run() {
    const char16_t cur[4] = { u'U', u'S', u'D', 0 };
    SV a({ cur, 3 });
    Q q({ 0, 7 });
    printf("%d %d\n", a.v, q.v);
    return 0;
}
}

namespace p_mp5 {
template <std::size_t N> struct W { static constexpr int v = N; };
template <typename... Ts> struct L { static constexpr int n = sizeof...(Ts); };
template <typename T, typename... Ls> struct A { int operator()() const { return T::v * 10 + L<Ls...>::n; } };
template <typename... Ls> int plain() { return A<W<3>, Ls...>{}(); }
template <std::size_t... Js> struct B {
  template <typename... Ls> static int one() { return (0 + ... + A<W<Js>, Ls...>{}()); }
};
template <std::size_t... Js> struct C {
  template <typename... Ls> static int one() { int r = 0; ((r += A<W<Js>, Ls...>{}()), ...); return r; }
};
int run() {
  printf("%d %d\n", plain<>(), plain<int>());
  printf("%d\n", C<1, 2>::one<>());
  printf("%d\n", B<1, 2>::one<>());
  return 0;
}
}

namespace p_ui1 {
struct S { int v; S(int x) : v(x) {} S(S&& o) : v(o.v) {} };
struct V {
    int got[4] = {};
    template<size_t position, typename U, typename... Items>
    void init(U&& item, Items&&... items) {
        init<position>(std::forward<U>(item));
        init<position + 1>(std::forward<Items>(items)...);
    }
    template<size_t position, typename U>
    void init(U&& value) { got[position] = value.v; }
    template<typename... Items> static V from(Items&&... items) { V r; r.template init<0>(std::forward<Items>(items)...); return r; }
};
int run() {
    V v = V::from(S(3), S(4));
    V w = V::from(S(7));
    printf("%d %d %d\n", v.got[0], v.got[1], w.got[0]);
    return 0;
}
}

namespace p_ub1 {
int run() {
    std::set<std::pair<unsigned long, unsigned long>> s = { {1, 2}, {3, 4}, {5, 6}, {7, 8} };
    auto it = std::upper_bound(s.begin(), s.end(), std::make_pair(4ul, 0), [](const auto& a, const auto& b) { return a.first < b.first; });
    auto lt = std::lower_bound(s.begin(), s.end(), std::make_pair(5ul, 0ul));
    std::vector<int> v = { 1, 3, 5, 7, 9 };
    auto vi = std::lower_bound(v.begin(), v.end(), 6);
    printf("%lu %lu %d %ld\n", it->first, lt->second, *vi, (long)std::distance(s.begin(), s.end()));
    return 0;
}
}

namespace p_ub2 {
int run() {
    std::set<int> s = { 1, 3, 5, 7 };
    printf("d %ld\n", (long)std::distance(s.begin(), s.end())); fflush(stdout);
    auto a = s.begin(); std::advance(a, 2); printf("a %d\n", *a); fflush(stdout);
    std::vector<int> v = { 1, 3, 5, 7, 9 };
    printf("dv %ld\n", (long)std::distance(v.begin(), v.end())); fflush(stdout);
    auto vi = std::lower_bound(v.begin(), v.end(), 6);
    printf("v %d\n", *vi); fflush(stdout);
    auto si = std::lower_bound(s.begin(), s.end(), 4);
    printf("s %d\n", *si); fflush(stdout);
    return 0;
}
}

namespace p_ub5 {
template <class It, class D> int adv(It &it, D n) {
    if constexpr (requires { it += n; }) return 1; else return 2;
}
template <class It, class D> int adv2(It it, D n) {
    if constexpr (requires { it += n; }) return 1; else return 2;
}
int run() {
    std::set<int> s = { 1, 3, 5, 7 };
    auto a = s.begin();
    int* p = 0;
    printf("%d %d %d\n", adv(a, 2), adv2(a, 2), adv(p, 2));
    return 0;
}
}

namespace p_cs2 {
enum J : uint8_t { A0, A1 = 59, A2, A3, A4 };
enum class C : uint8_t { None, Num, Big };
constexpr C f(J t) {
    switch (t) {
    case A1:
    case A2:
        return C::Big;
    case A3:
    case A4:
        return C::Num;
    default:
        return C::None;
    }
}
static_assert(f(A1) == C::Big);
static_assert(f(A2) == C::Big);
static_assert(f(A4) == C::Num);
static_assert(f(A0) == C::None);
int run() { printf("%d\n", (int)f(A2)); return 0; }
}

int main() {
    p_ev1::run();
    p_ev3::run();
    p_rc1::run();
    p_bl1::run();
    p_bl2::run();
    p_mp5::run();
    p_ui1::run();
    p_ub1::run();
    p_ub2::run();
    p_ub5::run();
    p_cs2::run();
    return 0;
}
