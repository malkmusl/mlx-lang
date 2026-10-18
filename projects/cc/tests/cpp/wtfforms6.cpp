// More WTF forms: unscoped enum qualifiers, this in member initializers, out-of-class member names, using-directive lookup, operator calls by name, constexpr arrays, alternative tokens, initializer_list objects, overload ranking
#include <stdio.h>
#include <utility>
#include <array>
#include <iterator>
#include <stdint.h>
#include <initializer_list>
#include <stddef.h>
#include <string>
#include <type_traits>
#include <concepts>
#include <span>

namespace p_ue1 {
namespace s { enum endianness { LITTLE = 0, BIG = 1, NATIVE = LITTLE }; constexpr bool m(endianness e) { return e == endianness::NATIVE; } }
void run() { printf("%d %d %d\n", s::m(s::LITTLE), s::m(s::endianness::BIG), (int)s::endianness::BIG); }
}

namespace p_ti1 {
struct L { L* prev { this }; L* next { nullptr }; int v = 3; int* pv { &v }; };
template<typename T> struct N { N* self { this }; T x {}; };
void run() { L l; N<int> n; L* h = new L; printf("%d %d %d %d %d\n", l.prev == &l, l.next == nullptr, *l.pv, n.self == &n, h->prev == h); delete h; }
}

namespace p_ti2 {
struct A { int a = 1; A* me { this }; int twice = a * 2; };
struct B : A { int pad = 5; B* self { this }; A* base { this }; B() : pad(7) { } int get() const { return pad + twice; } };
struct C { virtual ~C() { } int k = 4; C* c { this }; };
void run() { B b; C c; printf("%d %d %d %d %d %d\n", b.me == (A*)&b, b.self == &b, b.base == (A*)&b, b.get(), c.c == &c, b.twice); }
}

namespace p_ms1 {
namespace n {
class E {
public:
    template<typename T> unsigned salt() const { return Salt<T>::value; }
private:
    template <typename Type> struct Salt;
};
template <> struct E::Salt<bool> { static constexpr unsigned value = 3; };
template <> struct E::Salt<int> { static constexpr unsigned value = 17; };
}
void run() { n::E e; printf("%u %u\n", e.salt<bool>(), e.salt<int>()); }
}

namespace p_ud1 {
namespace W {
namespace Impl { class Object { public: int v = 7; }; template<typename T> int twice(T t) { return 2 * t; } }
namespace J { using namespace W::Impl; }
}
namespace W { int get() { J::Object o; return o.v + J::twice(4); } }
void run() { printf("%d\n", W::get()); W::J::Object o; printf("%d\n", o.v); }
}

namespace p_rd8 {
template<typename T> struct RawT { static int id() { return 1; } };
template<typename T> struct DefT { static int id() { return 2; } };
template<typename T, typename = RawT<T>, typename = DefT<T>> class Ref;
template<typename T, typename P, typename D> class Ref { public: T* p; };
template<typename T, typename P, typename D> class RefPtr;
template<typename T, typename _P, typename _D>
class RefPtr {
public:
    using PT = _P;
    RefPtr() { }
    template<typename X, typename Y> RefPtr(Ref<X, Y>&& r) : p(r.p) { printf("ctor %d\n", PT::id()); }
    RefPtr& operator=(const RefPtr&) { return *this; }
    RefPtr& operator=(T*) { return *this; }
    template<typename X, typename Y, typename Z> RefPtr& operator=(const RefPtr<X, Y, Z>&) { return *this; }
    RefPtr& operator=(RefPtr&&) { return *this; }
    template<typename X, typename Y, typename Z> RefPtr& operator=(RefPtr<X, Y, Z>&&) { return *this; }
    template<typename X> RefPtr& operator=(Ref<X>&&);
    T* p = nullptr;
    void reset();
};
template<typename X, typename Y> RefPtr(Ref<X, Y>&&) -> RefPtr<X, Y, DefT<X>>;
template<typename T, typename V, typename W>
template<typename U>
inline RefPtr<T, V, W>& RefPtr<T, V, W>::operator=(Ref<U>&& reference)
{
    RefPtr ptr = std::move(reference);
    p = ptr.p;
    return *this;
}
template<typename T, typename U, typename V> void RefPtr<T, U, V>::reset() { p = nullptr; }
struct B { int v = 5; static Ref<B> make(); };
B gb;
Ref<B> B::make() { Ref<B> r; r.p = &gb; return r; }
struct H { RefPtr<B, RawT<B>, DefT<B>> m; };
void f(RefPtr<B, RawT<B>, DefT<B>>& a, RefPtr<B, RawT<B>, DefT<B>>& b) {
 a = B::make();
 b = B::make(); }
void run() { H h, k; f(h.m, k.m); printf("%d\n", h.m.p->v + k.m.p->v); }
}

namespace p_rd9 {
template<typename A> struct Id { static int id() { return sizeof(A); } };
template<typename T, typename P, typename D> struct S {
    int one(); int two(); int three();
    template<typename X> int four(X);
};
template<typename T, typename V, typename W> int S<T, V, W>::one() { return Id<V>::id() * 100 + Id<W>::id(); }
template<typename T, typename U, typename V> int S<T, U, V>::two() { return Id<U>::id() * 100 + Id<V>::id(); }
template<typename A, typename B, typename C> template<typename X> int S<A, B, C>::four(X) { return Id<B>::id() * 100 + Id<X>::id(); }
template<typename T, typename V, typename W> int S<T, V, W>::three() { return Id<V>::id() * 100 + Id<W>::id(); }
void run() { S<char, short, long> s; printf("%d %d %d %d\n", s.one(), s.two(), s.three(), s.four(1)); }
}

namespace p_op1 {
struct M { int v; M operator*(int k) const { return M{v * k}; } M& operator=(const M& o) { v = o.v + 1; return *this; } bool operator==(const M& o) const { return v == o.v; } explicit operator bool() const { return v != 0; } };
inline M operator*(int l, const M& r) { return r.operator*(l); }
void run() { M a{3}; M b = 4 * a; M* p = &b; p->operator=(a); printf("%d %d %d %d\n", (4 * a).v, b.v, a.operator==(M{3}), (int)a.operator bool()); }
}

namespace p_ib1 {
struct Shape { int a; constexpr Shape(int x) : a(x) { } };
struct Str : Shape { static constexpr int s_flag = 7; enum { E1 = 40 }; Str() : Shape(1) { } };
struct Uniq : Str { };
struct Sym : Uniq {
    struct Static : Shape { constexpr Static(int k); int b; int get() const { return s_flag + E1 + a; } };
};
inline constexpr Sym::Static::Static(int k) : Shape(s_flag + k), b(E1) { }
void run() { Sym::Static s(2); printf("%d %d %d\n", s.a, s.b, s.get()); }
}

namespace p_rb1 {
struct C { std::array<char16_t, 4> a { u'x', u'y', 0, 0 }; char16_t b[3] = { 1, 2, 0 };
  unsigned len() const { unsigned r = 0; for (auto it = std::rbegin(a); it != std::rend(a); ++it) r += *it != 0; for (auto it = std::rbegin(b); it != std::rend(b); ++it) r += *it; return r; } };
void run() { C c; printf("%u\n", c.len()); }
}

namespace p_ca1 {
namespace a { namespace { namespace b {
constexpr uint8_t tbl[] = { 1, 2, 64, 4 };
static_assert(sizeof(tbl) == 4, "n");
static_assert(tbl[uint8_t(2)] == 64, "x");
static_assert(tbl[1] == 2);
} } }
void run() { printf("%d\n", a::b::tbl[2]); }
}

namespace p_alt1 {
void run() { int a = 5, b = 3; bool t = a > 1 and b > 1; bool u = not (a == b) or false; int c = a bitand b; c xor_eq 1; printf("%d %d %d %d %d\n", t, u, c, compl 0, a not_eq b); }
}

namespace p_il2 {
static int sum(std::initializer_list<int> l) { int s = 0; for (int v : l) s += v; return s; }
const std::initializer_list<int>& stat() { static const std::initializer_list<int> l{4, 5, 6,}; return l; }
void run() { std::initializer_list<int> a{1, 2, 3}; std::initializer_list<int> b = {7}; printf("%d %d %d %d %zu\n", sum(a), sum(b), sum(stat()), sum({}), a.size()); }
}

namespace p_au1 {
void run() { char b[4]; char *p = b; const auto s{p}; auto n{5}; printf("%d %d\n", (int)(s - b), n); }
}

namespace p_ec1 {
enum error_code { SUCCESS = 0, SURROGATE = 5 };
struct result { error_code error; size_t count; constexpr result(error_code e, size_t p) noexcept : error{e}, count{p} {} };
result f(bool s, size_t n) { return {s ? error_code::SURROGATE : error_code::SUCCESS, n}; }
void run() { result r = f(true, 3); printf("%d %zu\n", (int)r.error, r.count); }
}

namespace p_ov1 {
namespace s { namespace { namespace u {
template <typename R, typename W> size_t cv(R data, size_t len, W out) { printf("tmpl\n"); return 0; }
inline size_t cv(const char32_t *buf, size_t len, char *out) { printf("plain\n"); return cv(reinterpret_cast<const uint32_t *>(buf), len, out); }
} } }
struct I { size_t f(const char32_t *buf, size_t len, char *o) const noexcept; };
size_t I::f(const char32_t *buf, size_t len, char *o) const noexcept { return s::u::cv(buf, len, o); }
void run() { char32_t b[2] = {}; char o[2]; I i; i.f(b, 1, o); }
}

namespace p_sv1 {
void run() { printf("%s\n", std::string({(char)97, (char)98}).c_str()); std::string s2({(char)99}); printf("%s\n", s2.c_str()); }
}

namespace p_rq3 {
namespace sd { namespace detail {
template <typename T> concept byte_like = std::is_same_v<T, char> || std::is_same_v<T, unsigned char>;
template <class InputPtr> concept indexes_into_byte_like = requires(InputPtr p) { { std::decay_t<decltype(p[0])>{} } -> sd::detail::byte_like; };
template <class InputPtr> concept index_assignable_from_char = requires(InputPtr p, char s) { { p[0] = s }; };
}
namespace scalar { namespace { namespace l2u {
template <typename InputPtr, typename OutputPtr>
  requires(sd::detail::indexes_into_byte_like<InputPtr> &&
           sd::detail::index_assignable_from_char<OutputPtr>)
size_t convert(InputPtr data, size_t len, OutputPtr out) { for (size_t i = 0; i < len; i++) out[i] = data[i]; return len; }
inline size_t convert(const char *buf, size_t len, char *o) { return convert(reinterpret_cast<const unsigned char *>(buf), len, o); }
} } } }
void run() { const char in[] = "abc"; char out[4] = {}; printf("%zu %s\n", sd::scalar::l2u::convert((const char*)in, 3, out), out); }
}

namespace p_il1 {
struct I { int v; };
static I one{1};
static const I *getOne() { return &one; }
static const std::initializer_list<const I *> &all() {
  static const std::initializer_list<const I *> ptrs{
      getOne(),
  };
  return ptrs;
}
void run() { for (auto p : all()) printf("%d %zu\n", p->v, all().size()); }
}

namespace p_ab3 {
namespace s {
template <typename T> concept bytes = requires(T t) { t.size(); };
size_t conv(const char *in, size_t len, char16_t *out) noexcept;
inline size_t conv(const bytes auto &in, std::span<char16_t> out) noexcept { return conv(reinterpret_cast<const char *>(in.data()), in.size(), out.data()); }
struct Impl { virtual size_t conv(const char *in, size_t len, char16_t *out) const noexcept { for (size_t i = 0; i < len; i++) out[i] = in[i]; return len; } };
const Impl *get() { static Impl i; return &i; }
size_t conv(const char *buf, size_t len, char16_t *out) noexcept { return get()->conv(buf, len, out); }
}
extern "C" { size_t c(const char *in, size_t n, char16_t *o) { return s::conv(in, n, o); } }
void run() { char16_t o[4]; size_t n = c("abc", 3, o); printf("%zu %d\n", n, (int)o[1]); }
}

int main() {
    p_ue1::run();
    p_ti1::run();
    p_ti2::run();
    p_ms1::run();
    p_ud1::run();
    p_rd8::run();
    p_rd9::run();
    p_op1::run();
    p_ib1::run();
    p_rb1::run();
    p_ca1::run();
    p_alt1::run();
    p_il2::run();
    p_au1::run();
    p_ec1::run();
    p_ov1::run();
    p_sv1::run();
    p_rq3::run();
    p_il1::run();
    p_ab3::run();
    return 0;
}
