// WTF forms: constrained partial specializations, nested requirements, default template arguments inside function bodies, friend function templates defined in classes, static member functions beside member templates, midpoint/gcd/lcm, const locals read by lambdas, .template members failing in SFINAE, concepts over failing aliases
#include <stdio.h>
#include <type_traits>
#include <utility>
#include <concepts>
#include <stddef.h>
#include <initializer_list>
#include <numeric>
#include <tuple>
#include <span>

namespace p_rcl2 {
template<typename A, typename B, typename E = void> struct M { static constexpr int v = 1; };
template<typename A, typename B> requires (std::is_rvalue_reference_v<B&&>) struct M<A, B> { static constexpr int v = 2; };
template<typename A, typename B> requires (sizeof(A) == 100) struct M<A*, B> { static constexpr int v = 3; };
void run() { printf("%d %d %d\n", M<int, int>::v, M<int, int&>::v, M<int*, int&>::v); }
}

namespace p_cm1 {
template<typename T> struct Insp { using SourceItemType = std::remove_reference_t<T>; };
template<typename F, typename D, typename S, typename E = void> struct Mapper {
  using Item = typename Insp<S>::SourceItemType;
  using R = typename std::invoke_result<F, Item&>::type;
  static int go(const F& f) { Item i{}; return f(i) + 1; }
};
template<typename F, typename D, typename S> requires (std::is_rvalue_reference_v<S&&>)
struct Mapper<F, D, S> {
  using Item = typename Insp<S>::SourceItemType;
  using R = typename std::invoke_result<F, Item&&>::type;
  static int go(const F& f) { Item i{}; return f(std::move(i)) + 2; }
};
void run() { auto f = [](int x) { return x + 10; }; printf("%d %d\n", Mapper<decltype(f), int, int>::go(f), Mapper<decltype(f), int, int&>::go(f)); }
}

namespace p_nr1 {
template<typename T> concept HashableWithMember = requires(const T& t) {
  requires std::equality_comparable<T>;
  { t.hash() } -> std::same_as<unsigned>;
};
template<typename T> struct DefaultHash { static constexpr int kind = 1; };
template<HashableWithMember T> struct DefaultHash<T> { static constexpr int kind = 2; };
struct A { unsigned hash() const { return 1; } bool operator==(const A&) const = default; };
struct B { unsigned hash() const { return 1; } };
template<typename T> concept Small = requires { requires sizeof(T) < 4; };
void run() { printf("%d %d %d %d %d\n", DefaultHash<A>::kind, DefaultHash<B>::kind, DefaultHash<int>::kind, (int)Small<char>, (int)Small<long>); }
}

namespace p_dv6 {
template<typename T> struct Box { T* p; };
template<typename T, size_t N = 0> struct Vec { T items[4]; };
struct G {};
template<typename T> struct TS {
  static int check() { return std::is_same_v<Vec<Box<T>>, Vec<Box<G>>> * 1000 + std::is_same_v<Vec<T>, Vec<G>> * 100 + std::is_same_v<Vec<T*>, Vec<G*>> * 10 + std::is_same_v<Vec<Box<T>*>, Vec<Box<G>*>>; }
};
void run() { printf("%d %d\n", TS<G>::check(), (int)sizeof(Vec<Box<G>>)); }
}

namespace p_dv1 {
namespace W {
struct Crash {}; struct Mal {};
template<typename T> struct Traits {};
template<typename T, typename PT = Traits<T>> struct Box { T* p; };
template<typename T, size_t N = 0, typename O = Crash, size_t M = 16, typename A = Mal> struct Vec { T items[4]; };
}
using W::Box;
struct G {};
template<typename T> struct TS {
  static constexpr int check() { return std::is_same_v<Box<T>, Box<G>> * 10 + std::is_same_v<W::Vec<Box<T>>, W::Vec<Box<G>>>; }
};
void run() { printf("%d\n", TS<G>::check()); }
}

namespace p_ft1 {
namespace W {
template<typename... Types> unsigned computeHash(const Types&...);
class Hasher {
public:
  template<typename... Types> friend unsigned computeHash(const Types&... values) { Hasher h; addArgs(h, values...); return h.m; }
private:
  static void addArgs(Hasher&) {}
  template<typename T, typename... R> static void addArgs(Hasher& h, const T& v, const R&... r) { h.m = h.m * 31 + (unsigned)v; addArgs(h, r...); }
  unsigned m = 7;
};
inline unsigned use() { return computeHash(1, 2, 3); }
}
void run() { printf("%u %u\n", W::use(), W::computeHash(5)); }
}

namespace p_ft2 {
struct H {
  static void addArgs(H&) {}
  template<typename T, typename... R> static void addArgs(H& h, const T& v, const R&... r) { h.m = h.m * 31 + (unsigned)v; addArgs(h, r...); }
  unsigned m = 7;
  template<typename... Types> static unsigned go(const Types&... values) { H h; addArgs(h, values...); return h.m; }
};
void run() { printf("%u\n", H::go(1, 2, 3)); }
}

namespace p_ft3 {
struct H {
  static int f(int x) { return x; }
  template<typename T> static int f(T* p) { return 100; }
  int g(int x) { return x + k; }
  template<typename T> int g(T* p) { return 200 + k; }
  int k = 1;
};
void run() { H h; int i; printf("%d %d %d %d %d\n", h.f(3), h.f(&i), H::f(4), h.g(5), h.g(&i)); }
}

namespace p_ft4 {
struct H {
  static int f(int x) { return x; }
  template<typename T> static int f(T* p) { return 100; }
};
struct J { template<typename T> static int f(T* p) { return 100; } };
void run() { H h; J j; int i; printf("%d %d %d\n", h.f(&i), j.f(&i), H::f(&i)); }
}

namespace p_mp1 {
void run() { int a[5] = {1,2,3,4,5}; printf("%g %d %d %u %d %ld %d\n", std::midpoint(0.25, 0.75), std::midpoint(2147483647, 2147483645), std::midpoint(-3, 4), std::midpoint(10u, 3u), *std::midpoint(a, a + 4), std::gcd(12L, 18), std::lcm(4, 6)); }
}

namespace p_cl1 {
void run() { constexpr double ratio = 0.25; const int k = 3; auto f = [](double x) { return x * ratio + k; }; printf("%g\n", f(4.0)); }
}

namespace p_sg1 {
template<typename, typename = void> inline constexpr bool HasGet = false;
template<typename T> inline constexpr bool HasGet<T, std::void_t<decltype(std::declval<T>().template get<0>())>> = true;
struct WithGet { template<int I> int get() const { return I; } };
void run() { printf("%d %d %d\n", (int)HasGet<WithGet>, (int)HasGet<std::pair<int, int>>, (int)HasGet<int*>); }
}

namespace p_nr2 {
template<typename T> concept Adaptable = sizeof(T) == 1;
template<typename C> using Elem = typename decltype(std::declval<C>().span())::element_type;
template<typename C> concept HasSpan = requires {
    requires Adaptable<Elem<C>>;
};
struct S { std::span<const char> span() const { return {}; } };
struct L { std::span<const long> span() const { return {}; } };
void run() { printf("%d %d %d\n", (int)HasSpan<S>, (int)HasSpan<L>, (int)HasSpan<int>); }
}

namespace p_nr3 {
template<typename T, typename = void> class Adapter;
template<typename T> concept Adaptable = requires { typename Adapter<T>; };
template<> class Adapter<char> { public: static constexpr int kind = 1; };
template<typename C> using Elem = typename decltype(std::declval<C>().span())::element_type;
template<typename C> concept HasSpan = requires { requires Adaptable<Elem<C>>; };
template<typename C> requires HasSpan<C> class Adapter<C> { public: static constexpr int kind = 2; };
struct S { std::span<char> span() const { return {}; } };
template<typename T> constexpr int kindOf() { if constexpr (requires { Adapter<T>::kind; }) return Adapter<T>::kind; else return 0; }
void run() { printf("%d %d %d\n", kindOf<char>(), kindOf<S>(), (int)HasSpan<int>); }
}

int main() {
    p_rcl2::run();
    p_cm1::run();
    p_nr1::run();
    p_dv6::run();
    p_dv1::run();
    p_ft1::run();
    p_ft2::run();
    p_ft3::run();
    p_ft4::run();
    p_mp1::run();
    p_cl1::run();
    p_sg1::run();
    p_nr2::run();
    p_nr3::run();
    return 0;
}
