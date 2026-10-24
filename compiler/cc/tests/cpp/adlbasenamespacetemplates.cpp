// Argument-dependent lookup of a function template called with explicit
// arguments beside a using-declaration (`using std::get; get<I>(x)` of
// WTF::apply_impl): the templates of the argument's namespace and of its
// base's namespace are both candidates (WebCore's Style::get for a
// BorderRadius, whose base RectCorners is in WebCore, which has get
// templates of its own), not the last namespace's alone.
#include <cstdio>
#include <tuple>
#include <utility>
#include <functional>
namespace W {
template<class F, class T, size_t... I> decltype(auto) apply_impl(F&& functor, T&& tupleLike, std::index_sequence<I...>)
{
    using std::get;
    return std::invoke(std::forward<F>(functor), get<I>(std::forward<T>(tupleLike))...);
}
template<class F, class T> decltype(auto) apply(F&& functor, T&& tupleLike)
{
    return apply_impl(std::forward<F>(functor), std::forward<T>(tupleLike), std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>> { });
}
}
namespace A {
template<typename T> class Corners {
public:
    Corners(T a, T b) : m_a(a), m_b(b) { }
    const T& a() const { return m_a; }
    const T& b() const { return m_b; }
private:
    T m_a, m_b;
};
struct Other { };
template<size_t I> int get(const Other&) { return 0; }
namespace S {
struct Value { int v; };
struct Radius : Corners<Value> {
    Radius(Value a, Value b) : Corners<Value> { a, b } { }
};
template<size_t I> const auto& get(const Radius& value)
{
    if constexpr (!I)
        return value.a();
    else
        return value.b();
}
}
}
namespace std {
template<> struct tuple_size<A::S::Radius> : integral_constant<size_t, 2> { };
template<size_t I> struct tuple_element<I, A::S::Radius> { using type = A::S::Value; };
}
int main()
{
    A::S::Radius r { { 3 }, { 4 } };
    int sum = W::apply([](const auto&... x) { return (0 + ... + x.v); }, r);
    printf("%d %d\n", sum, W::apply_impl([](int x) { return x; }, A::Other { }, std::index_sequence<0> { }));
    return 0;
}
