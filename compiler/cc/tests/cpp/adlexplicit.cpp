// An unqualified call of a function template with its template arguments
// given, `get<I>(x)`, joins the templates of the name in the namespaces
// of its arguments (argument-dependent lookup): WTF::apply's `using
// std::get; get<I>(tupleLike)` finds WebCore's get of a
// SpaceSeparatedPoint.
#include <stdio.h>
#include <functional>
#include <tuple>
#include <utility>

namespace W {
template<class F, class T, size_t... I> constexpr decltype(auto) applyImpl(F &&functor, T &&tupleLike, std::index_sequence<I...>)
{
    using std::get;
    return std::invoke(std::forward<F>(functor), get<I>(std::forward<T>(tupleLike))...);
}
template<class F, class T> constexpr decltype(auto) apply(F &&functor, T &&tupleLike)
{
    return applyImpl(std::forward<F>(functor), std::forward<T>(tupleLike), std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>> { });
}
}

namespace C {
template<typename T> struct Pair { T a; T b; };
template<size_t I, typename T> constexpr const T &get(const Pair<T> &p) { if constexpr (!I) return p.a; else return p.b; }
template<typename T> struct Point { Pair<T> value; };
template<size_t I, typename T> constexpr decltype(auto) get(const Point<T> &point) { return get<I>(point.value); }
struct Tag { };
template<typename T> int pick(Tag) { return 2; }
}
namespace std {
template<typename T> struct tuple_size<C::Point<T>> : integral_constant<size_t, 2> { };
template<size_t I, typename T> struct tuple_element<I, C::Point<T>> { using type = T; };
}

namespace D {
template<typename T> int pick(long) { return 1; }
int f(const C::Point<int> &p) { using std::get; return get<1>(p); }
int g() { return pick<int>(C::Tag { }) * 10 + pick<int>(5); }
}

int main(int argc, char **argv)
{
    C::Point<int> p { { 3, 4 } };
    printf("%d\n", W::apply([](int a, int b) { return a * 10 + b; }, p));
    std::tuple<int, int> t { 5, 6 };
    printf("%d\n", W::apply([](int a, int b) { return a * 10 + b; }, t));
    printf("%d %d\n", D::f(p), D::g());
    return argc;
}
