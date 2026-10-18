// Probes of the WTF work (each its own namespace), compared with g++.
#include <stdio.h>
#include <type_traits>
#include <utility>
#include <span>
#include <limits>

namespace nn1 {
template <typename From, typename To> struct nnc {
  template <typename T> static std::true_type test(T(&&)[1]);
  template <typename T> static auto impl(int) -> decltype(test<T>({std::declval<From>()}));
  template <typename> static auto impl(...) -> std::false_type;
  static constexpr bool value = decltype(impl<To>(0))::value;
};
int f(int) { return 1; } int f(char *) { return 2; }
int g(long) { return 1; }
void run() { printf("%d %d %d %d %d %d %d\n", nnc<int,int>::value, nnc<double,int>::value, nnc<int,double>::value, nnc<float,double>::value, nnc<int,long>::value, nnc<const char *, bool>::value, f({3}) + g({4})); }

}

namespace vs1 {
template <class T, class = void> struct HasRef : std::false_type {};
template <class T> struct HasRef<T, std::void_t<decltype(std::declval<T>().ref())>> : std::true_type {};
struct A { void ref() {} }; struct B {};
void run() { printf("%d %d\n", HasRef<A>::value, HasRef<B>::value); }

}

namespace vt1 {
template<typename, typename = void> inline constexpr bool IsTypeComplete = false;
template<typename T> inline constexpr bool IsTypeComplete<T, std::void_t<decltype(sizeof(T))>> = true;
struct Inc; struct Com { int x; };
void run() { printf("%d %d %d\n", IsTypeComplete<Com>, IsTypeComplete<Inc>, IsTypeComplete<int>); }

}

namespace ir8 {
template <typename C, typename F> requires (std::is_invocable_r_v<bool, F, C>)
inline int find(std::span<const C> s, F &&f, unsigned start) { return 1; }
template <typename C> inline int find(std::span<const C> s, C c, unsigned start) { return 2; }
void run() { const char d[3] = {'a','b','c'}; std::span<const char> s(d, 3); char c = 'b';
  printf("%d %d %d %d\n", std::is_invocable_r_v<bool, char &, char>, std::is_invocable_r_v<bool, bool (*)(char), char>, find(s, c, 0u), find(s, [](char x) { return x == 'b'; }, 0u)); }

}

namespace ir7 {
template <typename From, typename To> struct nnc {
  template <typename T> static std::true_type test(T(&&)[1]);
  template <typename T> static auto impl(int) -> decltype(test<T>({std::declval<From>()}));
  template <typename> static auto impl(...) -> std::false_type;
  static constexpr bool value = decltype(impl<To>(0))::value;
};
template <typename A, typename T, typename = void> struct L { static constexpr int v = 0; };
template <typename A, typename T> struct L<A, T, std::enable_if_t<nnc<A, T>::value>> { static constexpr int v = 1; };
template <typename A, typename T> struct M { static constexpr int v = 0; };
template <typename A> struct M<A, std::enable_if_t<std::is_same<std::remove_cvref_t<A>, bool>::value ? true : nnc<A, int>::value, int>> { static constexpr int v = 1; };
void run() { printf("%d %d %d %d %d\n", nnc<int, int>::value, L<int, int>::v, L<double, int>::v, M<int, int>::v, M<double, int>::v); }

}

namespace bc3 {
template<typename T, bool B = sizeof(T) >= 4> struct X;
template<typename T> struct X<T, true> { static int f() { return 1; } };
template<typename T> struct X<T, false> { static int f() { return 0; } };
template<typename T, bool S = std::numeric_limits<T>::is_signed> struct Y;
template<typename T> struct Y<T, true> { static int f() { return 1; } };
template<typename T> struct Y<T, false> { static int f() { return 0; } };
void run() { printf("%d %d %d %d\n", X<int>::f(), X<char>::f(), Y<int>::f(), Y<unsigned>::f()); }

}

namespace rr1 {
template <class T> struct R { T *p; R(T &o) : p(&o) {} R(R &&o) : p(o.p) {} };
struct S { int v; S(int x, R<S> &&base) : v(x + base.p->v) {} S(int x) : v(x) {} };
int f(R<S> &&r) { return r.p->v; }
int g(const R<S> &r) { return 1; } int g(R<S> &&r) { return 2; }
void run() { S a(5); S b(1, a); R<S> keep(a); printf("%d %d %d %d\n", b.v, f(a), g(a), g(keep)); }

}

namespace sc1 {
template <std::size_t I> using sc = std::integral_constant<std::size_t, I>;
template <typename Arg, std::size_t I, typename T> struct leaf { using impl = sc<I> (*)(T); operator impl() const { return nullptr; } };
template <typename Arg, typename... Ts> struct ov {
  template <typename> struct impl;
  template <std::size_t... Is> struct impl<std::index_sequence<Is...>> : leaf<Arg, Is, Ts>... {};
  using type = impl<std::index_sequence_for<Ts...>>;
};
template <typename Arg, typename... Ts> using best = decltype(std::declval<typename ov<Arg, Ts...>::type>()(std::declval<Arg>()));
static sc<7> seven(int) { return {}; }
struct F { using P = sc<7> (*)(int); operator P() const { return seven; } };
void run() { F f; printf("%zu %zu %zu %zu\n", best<int, char *, int, double>::value, best<double, int, double>::value, best<const char *, const char *, int>::value, decltype(f(1))::value + f(2).value); }

}

namespace ir2 {
#define RET(...) noexcept(noexcept(__VA_ARGS__)) -> decltype(__VA_ARGS__) { return __VA_ARGS__; }
namespace lib {
template <typename T> struct identity { using type = T; };
template <typename...> using void_t = void;
namespace detail {
template <typename F, typename... Args> inline constexpr auto invoke(F &&f, Args &&... args) RET(std::forward<F>(f)(std::forward<Args>(args)...))
}
template <typename F, typename... Args> inline constexpr auto invoke(F &&f, Args &&... args) RET(detail::invoke(std::forward<F>(f), std::forward<Args>(args)...))
namespace detail {
template <typename Void, typename, typename...> struct invoke_result {};
template <typename F, typename... Args> struct invoke_result<void_t<decltype(lib::invoke(std::declval<F>(), std::declval<Args>()...))>, F, Args...> : identity<decltype(lib::invoke(std::declval<F>(), std::declval<Args>()...))> {};
}
template <typename F, typename... Args> using invoke_result = detail::invoke_result<void, F, Args...>;
template <typename F, typename... Args> using invoke_result_t = typename invoke_result<F, Args...>::type;
}
template <std::size_t I> using size_constant = std::integral_constant<std::size_t, I>;
template <typename From, typename To> struct nnc {
  template <typename T> static std::true_type test(T(&&)[1]);
  template <typename T> static auto impl(int) -> decltype(test<T>({std::declval<From>()}));
  template <typename> static auto impl(...) -> std::false_type;
  static constexpr bool value = decltype(impl<To>(0))::value;
};
template <typename Arg, std::size_t I, typename T, bool = std::is_arithmetic<T>::value, typename = void> struct overload_leaf {};
template <typename Arg, std::size_t I, typename T> struct overload_leaf<Arg, I, T, false> { using impl = size_constant<I> (*)(T); operator impl() const { return nullptr; }; };
template <typename Arg, std::size_t I, typename T> struct overload_leaf<Arg, I, T, true, std::enable_if_t<std::is_same<std::remove_cvref_t<T>, bool>::value ? std::is_same<std::remove_cvref_t<Arg>, bool>::value : nnc<Arg, T>::value>> { using impl = size_constant<I> (*)(T); operator impl() const { return nullptr; }; };
template <typename Arg, typename... Ts> struct overload_impl {
  private: template <typename> struct impl;
  template <std::size_t... Is> struct impl<std::index_sequence<Is...>> : overload_leaf<Arg, Is, Ts>... {};
  public: using type = impl<std::index_sequence_for<Ts...>>;
};
template <typename Arg, typename... Ts> using overload = typename overload_impl<Arg, Ts...>::type;
template <typename Arg, typename... Ts> using best_match = lib::invoke_result_t<overload<Arg, Ts...>, Arg>;
void run() { printf("%zu %zu %zu\n", best_match<int, int, double>::value, best_match<double, int, double>::value, best_match<const char *, bool, const char *>::value); }

}

namespace sw1 {
struct B {
  template <std::size_t I, typename V> static int step_at(V &&v) { return (int)I * 10 + v; }
  template <std::size_t Base, std::size_t N, typename V, typename... Vs>
  static int switch_at(std::size_t index, V &&v, Vs &&... vs) {
#define C(I) case Base + I: if constexpr (Base + I < N) return step_at<(Base + I < N ? Base + I : 0)>(std::forward<V>(v)); else __builtin_unreachable();
    switch (index) {
      C(0) C(1) C(2)
      default:
        if constexpr (Base + 3 < N) return switch_at<Base + 3, N>(index, std::forward<V>(v), std::forward<Vs>(vs)...);
        else __builtin_unreachable();
    }
#undef C
  }
};
void run() { printf("%d %d\n", B::switch_at<0, 5>(1, 2), B::switch_at<0, 5>(4, 3)); }

}

namespace tm2 {
template <typename V> struct W {
  V &&v_;
  template <typename... A> auto operator()(A &&... a) const -> decltype(std::forward<V>(v_)(std::forward<A>(a)...)) { return std::forward<V>(v_)(std::forward<A>(a)...); }
};

void run() { auto f = [](int a, int b) { return a + b; }; W<decltype(f) &> w{f}; printf("%d\n", w(2, 3)); }

}

int main() {
    nn1::run();
    vs1::run();
    vt1::run();
    ir8::run();
    ir7::run();
    bc3::run();
    rr1::run();
    sc1::run();
    ir2::run();
    sw1::run();
    tm2::run();
    return 0;
}
