// An operator template of the namespace of a class's template argument,
// found by argument-dependent lookup: WebCore::Style's operator== of
// WTF::UniqueRef<RelativeColor<D>>, reached from mpark::variant's
// equal_to through an invoke_result.
#include <cstdio>
#include <utility>
#include <type_traits>
namespace W {
template<typename T> struct UniqueRef {
    T* m;
    T& get() const { return *m; }
    operator T&() const { return *m; }
};
}
namespace M {
struct equal_to {
    template<typename L, typename R>
    constexpr auto operator()(L&& l, R&& r) const -> decltype(std::forward<L>(l) == std::forward<R>(r)) { return std::forward<L>(l) == std::forward<R>(r); }
};
template<typename V, typename F, typename... A> struct result {};
template<typename F, typename... A> struct result<std::void_t<decltype(std::declval<F>()(std::declval<A>()...))>, F, A...> { using type = decltype(std::declval<F>()(std::declval<A>()...)); };
template<typename F, typename... A> using result_t = typename result<void, F, A...>::type;
template<typename T> bool same(const T& a, const T& b) {
    using R = result_t<equal_to, const T&, const T&>;
    return R(equal_to {}(a, b));
}
}
namespace N {
struct D { };
namespace S {
template<typename X> struct Rel { int v; bool operator==(const Rel& o) const { return v == o.v; } };
}
}
bool check(const W::UniqueRef<N::S::Rel<N::D>>& a, const W::UniqueRef<N::S::Rel<N::D>>& b) { return M::same(a, b); }
namespace N { namespace S {
template<typename X> bool operator==(const W::UniqueRef<Rel<X>>& a, const W::UniqueRef<Rel<X>>& b) { return a.get() == b.get(); }
} }
int main() {
    N::S::Rel<N::D> x { 3 }, y { 3 }, z { 4 };
    W::UniqueRef<N::S::Rel<N::D>> a { &x }, b { &y }, c { &z };
    std::printf("%d %d\n", check(a, b), check(a, c));
    std::printf("%d %d\n", a == b, a == c);
    return 0;
}
