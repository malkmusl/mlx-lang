// WTF forms: injected class names as qualifiers, std::nullptr_t, static and local constexpr auto, pack expansions in aggregate braces, deduction for globals, overloads across using-declared bases, implicit assignment by members (moved from rvalues, arrays), members of rvalues, this in generic lambdas, constructors inherited through an alias, class members hiding using-directives
#include <stdio.h>
#include <stddef.h>
#include <cstddef>
#include <type_traits>
#include <memory>
#include <functional>
#include <array>
#include <span>
#include <utility>

namespace p_ip1 {
enum class Tag { A, B };
template<Tag tag, int kind = 0> class CP {
public:
    CP() = default;
    static CP fromRaw(char* p) { CP r; r.m = p; return r; }
    template<typename T = void*> T raw() const { return (T)m; }
    CP operator+(size_t n) const { return CP::fromRaw(raw<char*>() + n); }
    CP& operator+=(size_t n) { return *this = *this + n; }
    char* m = nullptr;
};
void run() { char buf[8]; auto p = CP<Tag::A>::fromRaw(buf); p += 3; printf("%d\n", (int)(p.m - buf)); }
}

namespace p_ip2 {
template<typename T> class CP {
public:
    static CP make(int v) { CP r; r.m = v; return r; }
    CP next() const { return CP::make(m + 1); }
    CP& bump() { return *this = next(); }
    int m = 0;
};
void run() { auto p = CP<int>::make(3); p.bump(); printf("%d\n", p.m); }
}

namespace p_np1 {
struct R { int k; R(std::nullptr_t) : k(1) {} explicit R(void* p) : k(2) {} template<typename U> R(U* p) : k(3) {} };
int f(std::nullptr_t) { return 10; }
int f(void*) { return 20; }
int g(int*) { return 1; }
template<typename T> int h(T*) { return 1; }
template<typename T> int h(T) { return 2; }
void run() {
  int x = 0; int* ip = nullptr; char* cp = nullptr;
  static_assert(!std::is_pointer_v<std::nullptr_t>);
  static_assert(std::is_null_pointer_v<std::nullptr_t>);
  static_assert(std::is_same_v<decltype(nullptr), std::nullptr_t>);
  std::unique_ptr<int> u = nullptr; std::function<void()> fn = nullptr;
  R a(nullptr), b((void*)&x), c(&x);
  printf("%d %d %d %d %d %d %d %d %d\n", f(nullptr), f((void*)0), g(nullptr), h(nullptr), h(&x), a.k, b.k, c.k, (int)(ip == nullptr && cp == nullptr && !u && !fn));
}
}

namespace p_cp3 {
enum PtrTag : unsigned { NoTag = 0, T1 = 41782 };
template<PtrTag tag, typename P> void assertTagged(P) {}
struct Base {};
template<PtrTag tag, int attr = 0> class CodePtr : public Base {
public:
    CodePtr() = default;
    CodePtr(std::nullptr_t) { }
    explicit CodePtr(void* value) : m_value(value) { assertTagged<tag>(value); }
    void* get() const { return m_value; }
private:
    void* m_value { nullptr };
};
using MemoryPtr = CodePtr<T1>;
struct H { MemoryPtr start() { return MemoryPtr(buf); } char buf[4]; };
void run() { H h; printf("%d\n", h.start().get() == h.buf); }
}

namespace p_ph1 {
enum class pfi { not_found, unique, repeated };
template<class P, pfi info> struct fpp { using policy = P; static constexpr auto found_info = info; };
template<bool r, class... F> struct fl { static constexpr bool repeated = r; };
template<class Base, class Def> struct bdp {
  template<class FI, class... Ps> struct impl;
  template<class FI> struct impl<FI> { using type = FI; };
  template<class FI, class First, class... Rest> struct impl<FI, First, Rest...> {
    using type = typename std::conditional<std::is_base_of<Base, First>::value,
      typename std::conditional<FI::found_info == pfi::not_found, typename impl<fpp<First, pfi::unique>, Rest...>::type, typename impl<fpp<First, pfi::repeated>, Rest...>::type>::type,
      typename impl<FI, Rest...>::type>::type;
  };
  template<class... Ps> using get = typename impl<fpp<Def, pfi::not_found>, Ps...>::type;
};
struct KB {}; struct KD : KB {}; struct KU : KB {};
template<class First, bool repeated, class... Ps> struct mk {
  using nf = typename First::template get<Ps...>;
  using type = fl<(repeated || nf::found_info == pfi::repeated), nf>;
};
void run() {
  using A = mk<bdp<KB, KD>, false, KU>::type; using B = mk<bdp<KB, KD>, false, KU, KU>::type; using C = mk<bdp<KB, KD>, false, int>::type;
  printf("%d %d %d\n", A::repeated, B::repeated, C::repeated);
}
}

namespace p_ph2 {
enum class pfi { not_found, unique, repeated };
template<class P, pfi info> struct fpp { static constexpr auto found_info = info; static constexpr pfi fi2 = info; };
template<bool B> struct bb { static constexpr bool v = B; };
void run() {
  printf("%d\n", bb<fpp<int, pfi::unique>::fi2 == pfi::unique>::v);
  printf("%d\n", bb<fpp<int, pfi::unique>::found_info == pfi::unique>::v);
}
}

namespace p_ph3 {
namespace d2b { enum class tag_t { to_nearest, left_closed }; }
struct Near { static constexpr auto tag = d2b::tag_t::to_nearest; };
struct Left { static constexpr d2b::tag_t tag = d2b::tag_t::left_closed; };
template<class P> int f() { constexpr auto tag = P::tag; if constexpr (tag == d2b::tag_t::to_nearest) { return 1; } else { return 2; } }
void run() { constexpr auto x = 3 + 4; static_assert(x == 7); printf("%d %d\n", f<Near>(), f<Left>()); }
}

namespace p_ep1 {
struct closed { static constexpr bool c = true; constexpr bool include_left_endpoint() const noexcept { return true; } };
struct sym { bool is_closed; constexpr bool include_left_endpoint() const noexcept { return is_closed; } };
template<class IT, class... A> int f(int x, A... a) { IT it { a... }; return it.include_left_endpoint() + x; }
void run() { printf("%d %d %d\n", f<closed>(1), f<sym>(1, true), f<sym>(1, false)); }
}

namespace p_ep3 {
struct sym { bool is_closed; int k; };
template<class... A> int f(A... a) { sym it { a... }; return it.is_closed + it.k; }
template<class... A> int g(A... a) { sym it = { a... }; return it.is_closed + it.k; }
void run() { printf("%d %d %d\n", f(true, 5), f(true), g(false, 2)); }
}

namespace p_sp1 {
static constexpr std::array tbl { '0', '1', '2', '3' };
void run() { auto s = std::span { tbl }.subspan(1, 2); printf("%c %zu\n", s[0], s.size()); }
}

namespace p_sp2 {
static constexpr std::array tbl { '0', '1', '2', '3' };
std::array t2 { 1, 2, 3 };
void run() { printf("%d %d\n", (int)std::is_same_v<decltype(t2), std::array<int, 3>>, (int)sizeof(tbl)); }
}

namespace p_np2 {
template<typename T> struct Box { T* p; };
template<typename T> bool operator==(const Box<T>& b, std::nullptr_t) { return b.p == nullptr; }
template<typename T> bool isNull(T* p, std::nullptr_t n) { return p == n; }
void run() { int x; Box<int> a{nullptr}, b{&x}; printf("%d %d %d\n", (int)(a == nullptr), (int)(b == nullptr), (int)isNull(&x, nullptr)); }
}

namespace p_vi2 {
template<class A, class... B> struct Visitor : Visitor<A>, Visitor<B...> {
    Visitor(A a, B... b) : Visitor<A>(a), Visitor<B...>(b...) { }
    using Visitor<A>::operator();
    using Visitor<B...>::operator();
};
template<class A> struct Visitor<A> : A { Visitor(A a) : A(a) { } using A::operator(); };
template<class... F> Visitor<F...> makeVisitor(F... f) { return Visitor<F...>(f...); }
template<typename F, typename... Args> auto invoke(F&& f, Args&&... args) -> decltype(std::forward<F>(f)(std::forward<Args>(args)...)) { return std::forward<F>(f)(std::forward<Args>(args)...); }
void run() {
  int n = 0;
  int k1 = 100, k2 = 200; double k3 = 0.5; auto v = makeVisitor([k1](bool b) { return b ? k1 : 2; }, [k2, k1](int i) { return i * 10 + k2 + k1; }, [k3](double d) { return (int)(d + k3); });
  bool t = false; int i = 3;
  printf("%d %d %d %d %d\n", v(true), v(4), v(2.7), invoke(v, t), invoke(std::move(v), i));
}
}

namespace p_da1 {
struct M { int k = 0; M() = default; M(const M&) = default; M& operator=(const M&) { k = 1; return *this; } M& operator=(M&&) { k = 2; return *this; } };
struct MO { int k = 0; MO() = default; MO& operator=(MO&&) { k = 3; return *this; } MO& operator=(const MO&) = delete; };
struct H { M m; MO mo; int x; H() = default; H& operator=(H&&) = default; H& operator=(const H&) = delete; };
struct I { M m; int x; };
void run() { H a, b; b.x = 5; a = std::move(b); I c, d; c = d; I e; e = std::move(d); printf("%d %d %d %d %d\n", a.m.k, a.mo.k, a.x, c.m.k, e.m.k); }
}

namespace p_va2 {
template<typename F, typename... A> constexpr auto myinvoke(F&& f, A&&... a) -> decltype(std::forward<F>(f)(std::forward<A>(a)...)) { return std::forward<F>(f)(std::forward<A>(a)...); }
template<std::size_t I, typename T> struct alt { T value; };
struct MO { int k = 0; MO(int v) : k(v) {} MO(MO&& o) : k(o.k) {} MO& operator=(MO&& o) { k = o.k + 100; return *this; } MO(const MO&) = delete; MO& operator=(const MO&) = delete; };
template<typename T> struct Base {
  alt<0, T> a0;
  template<typename V, typename... Vs> using result_t = decltype(myinvoke(std::declval<V>(), std::declval<Vs>().a0...));
  template<typename V, typename... Vs> static result_t<V, Vs...> visit_at(V&& v, Vs&&... vs) { return std::forward<V>(v)(std::forward<Vs>(vs).a0...); }
  template<std::size_t I, typename U, typename Arg> void assign_alt(alt<I, U>& a, Arg&& arg) { a.value = std::forward<Arg>(arg); }
  template<typename That> void generic_assign(That&& that) {
    visit_at([this](auto& this_alt, auto&& that_alt) { this->assign_alt(this_alt, std::forward<decltype(that_alt)>(that_alt).value); }, *this, std::forward<That>(that));
  }
};
void run() { Base<MO> x{{MO(1)}}, y{{MO(2)}}; x.generic_assign(std::move(y)); printf("%d\n", x.a0.value.k); }
}

namespace p_va4 {
template<typename F, typename... A> constexpr auto myinvoke(F&& f, A&&... a) -> decltype(std::forward<F>(f)(std::forward<A>(a)...)) { return std::forward<F>(f)(std::forward<A>(a)...); }
template<std::size_t I, typename T> struct alt { T value; };
struct MO { int k = 0; MO(int v) : k(v) {} MO(MO&& o) : k(o.k) {} MO& operator=(MO&& o) { k = o.k + 100; return *this; } MO(const MO&) = delete; MO& operator=(const MO&) = delete; };
template<typename T> struct Base {
  alt<0, T> a0;
  template<typename V, typename... Vs> using result_t = decltype(myinvoke(std::declval<V>(), std::declval<Vs>().a0...));
  template<typename V, typename... Vs> static result_t<V, Vs...> visit_at(V&& v, Vs&&... vs) { return std::forward<V>(v)(std::forward<Vs>(vs).a0...); }
  template<std::size_t I, typename U, typename Arg> void assign_alt(alt<I, U>& a, Arg&& arg) { a.value = std::forward<Arg>(arg); }
  template<typename That> void generic_assign(That&& that) {
    visit_at([this](alt<0, T>& this_alt, alt<0, T>&& that_alt) { this->assign_alt(this_alt, std::forward<decltype(that_alt)>(that_alt).value); }, *this, std::forward<That>(that));
  }
};
void run() { Base<MO> x{{MO(1)}}, y{{MO(2)}}; x.generic_assign(std::move(y)); printf("%d\n", x.a0.value.k); }
}

namespace p_gl1 {
struct S {
  int base = 10;
  int add(int x) const { return base + x; }
  int go() const { auto f = [this](auto v) { return add(v) + this->base + base; }; return f(1) + f(2); }
  int go2() { auto f = [&](auto v) { base += v; return add(v); }; return f(5); }
};
void run() { S s; printf("%d %d\n", s.go(), s.go2()); }
}

namespace p_da2 {
struct M { int k = 0; M& operator=(const M& o) { k = o.k + 1; return *this; } M& operator=(M&& o) { k = o.k + 10; return *this; } };
struct H { M m; int arr[3]; M ms[2]; char tail; };
void run() { H a{}, b{}; b.arr[0] = 4; b.arr[2] = 6; b.ms[1].k = 3; b.tail = 'z'; a = b; printf("%d %d %d %d %c\n", a.m.k, a.arr[0], a.arr[2], a.ms[1].k, a.tail); H c{}; c = std::move(b); printf("%d %d\n", c.ms[1].k, c.arr[2]); }
}

namespace p_ih1 {
struct Impl { int v; };
template<typename T> struct AlignedPtr { T* m; constexpr AlignedPtr() : m(nullptr) {} constexpr AlignedPtr(std::nullptr_t) : m(nullptr) {} AlignedPtr(T* value) : m(value) {} T* get() const { return m; } };
struct Deleted {};
template<typename T> struct Packed : public AlignedPtr<T> { using Base = AlignedPtr<T>; using Base::Base; Packed(Deleted) : Base((T*)1) {} };
template<typename T> struct Set { int n = 0; int add(const T& e) { n += 1; return e.get()->v; } int add(T&& e) { n += 10; return e.get()->v; } };
int take(Packed<Impl> p) { return p.get()->v; }
void run() { Set<Packed<Impl>> s; Impl i{3}; int r = s.add(&i); Packed<Impl> q = &i; printf("%d %d %d %d\n", s.n, r, take(&i), q.get()->v); }
}

namespace p_ae1 {
namespace A {
namespace U { inline int equal(int a, int b) { return a == b ? 10 : 20; } }
using namespace U;
struct T {
  int k = 1;
  static int equal(const char* s, int x) { return s[0] == 'a' ? x : -x; }
  static int twice(int x) { return equal("a", x) * 2; }
  int member(int x) { return k + x; }
};
template<typename Tr> struct Adapter { static int eq(const char* s, int x) { return Tr::equal(s, x); } };
inline int g() { return equal(3, 3); }
}
void run() { printf("%d %d %d %d\n", A::T::twice(4), A::Adapter<A::T>::eq("b", 5), A::g(), A::T::equal("a", 7)); }
}

int main() {
    p_ip1::run();
    p_ip2::run();
    p_np1::run();
    p_cp3::run();
    p_ph1::run();
    p_ph2::run();
    p_ph3::run();
    p_ep1::run();
    p_ep3::run();
    p_sp1::run();
    p_sp2::run();
    p_np2::run();
    p_vi2::run();
    p_da1::run();
    p_va2::run();
    p_va4::run();
    p_gl1::run();
    p_da2::run();
    p_ih1::run();
    p_ae1::run();
    return 0;
}
