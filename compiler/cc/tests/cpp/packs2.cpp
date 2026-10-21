// Probes of the WTF work (each its own namespace), compared with g++.
#include <stdio.h>
#include <stddef.h>
#include <array>
#include <utility>
#include <type_traits>

namespace pk5 {
template <class... Ts> struct tl {};
template <typename Arg> struct ov {
  template <typename> struct impl;
  template <typename... Is> struct impl<tl<Is...>> { static constexpr int n = sizeof...(Is); };
  template <typename U> struct impl<U*> { static constexpr int n = 100; U u; };
};
void run() { printf("%d %d %d %d\n", ov<int>::impl<tl<int, int>>::n, ov<double>::impl<tl<int, int, int>>::n, ov<int>::impl<int*>::n, ov<double>::impl<char*>::n); }

}

namespace al8 {
template <typename... Ts> struct tl {};
template <unsigned I, typename T> struct alt { T value; };
template <typename... Ts> class base {
public:
    template <unsigned I> static int f(const tl<Ts...> &a) { return I; }
    template <unsigned I> using at = alt<I, tl<Ts...>>;
    template <unsigned I> static int g(const at<I> &a) { return I; }
};
void run() { tl<char, int> x; alt<2, tl<char, int>> y; printf("%d %d\n", base<char, int>::f<3>(x), base<char, int>::g<2>(y)); }

}

namespace br1 {
template <typename T, size_t N> struct arr { T data[N == 0 ? 1 : N]; constexpr const T &operator[](size_t i) const { return data[i]; } };
struct V { bool b; bool f() const { return b; } };
template <typename... Vs> bool all(Vs &&... vs) { arr<bool, sizeof...(Vs)> a{{!vs.f()...}}; bool r = true; for (size_t i = 0; i < sizeof...(Vs); ++i) r = r && a[i]; return r; }
template <typename... Vs> int cnt(Vs... vs) { return (int)(arr<int, sizeof...(Vs)>{{vs...}}[1]); }
void run() { V x{false}, y{false}, z{true}; printf("%d %d %d\n", all(x, y), all(x, z), cnt(4, 5, 6)); }

}

namespace br2 {
template <typename T, size_t N> struct arr { constexpr const T &operator[](size_t i) const { return data[i]; } std::array<T, N == 0 ? 1 : N> data; };
template <typename... Vs> int sum(Vs... vs) { arr<int, sizeof...(Vs)> a{{vs...}}; int r = 0; for (size_t i = 0; i < sizeof...(Vs); ++i) r += a[i]; return r; }
template <typename... Vs> int none(Vs... vs) { int a[] = {0, vs...}; return sizeof(a) / sizeof(int); }
void run() { printf("%d %d %d %d\n", sum(1, 2, 3), sum(5), none(), none(1, 2)); }

}

namespace dp1 {
template <class... Ts> struct tl { static constexpr int n = sizeof...(Ts); static constexpr int refs = (0 + ... + (std::is_reference<Ts>::value ? 1 : 0)); };
struct A { int value; };
template <class... Alts> auto f(Alts &&... alts) -> decltype(tl<decltype((std::forward<Alts>(alts).value))...>{}) { return {}; }
template <class... Alts> int g(Alts &&... alts) { return tl<decltype((std::forward<Alts>(alts).value))...>::refs; }
void run() { A a{1}, b{2}; auto r = f(a, b); printf("%d %d %d\n", decltype(r)::n, decltype(r)::refs, g(a, A{3})); }

}

int main() {
    pk5::run();
    al8::run();
    br1::run();
    br2::run();
    dp1::run();
    return 0;
}
