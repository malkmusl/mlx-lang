// std::is_constructible<C, T> where T converts to C through its
// conversion operator and C's copy and move constructors are defaulted
// or implicit, beside constructor templates that do not take T (WTF's
// Variant from WebAnimationTime, whose `operator CSSNumberish()` makes
// one: std::optional<CSSNumberish> of an std::optional<WebAnimationTime>
// through optional's converting constructor).
#include <cstdio>
#include <optional>
#include <type_traits>
#include <utility>

struct V { V(const V&) = default; V(V&&) = default; V(int x) : x(x) {} int x; };
struct W { W(const W&) = default; W(W&&) = default; template<class U> requires std::is_convertible_v<U, int> W(U&& u) : x((int)u) {} int x; };
template<class... Ts> struct X { X(const X&) = default; X(X&&) = default; template<class U, class = std::enable_if_t<std::is_convertible_v<U, int>>> X(U&& u) : x((int)u) {} int x; };
struct NoCopy { NoCopy(const NoCopy&) = delete; NoCopy(NoCopy&&) = delete; template<class U> requires std::is_convertible_v<U, int> NoCopy(U&& u) : x((int)u) {} int x; };
struct T {
    operator V() const { return V(3); }
    operator W() const { return W(4); }
    operator X<int>() const { return X<int>(5); }
    operator NoCopy() const { return NoCopy(6); }
};

template<typename U> static int convert(U&& value)
{
    std::optional<X<int>> converted(std::forward<U>(value));
    return converted ? converted->x : -1;
}

int main()
{
    printf("%d %d %d %d\n", (int)std::is_constructible_v<V, T&&>, (int)std::is_constructible_v<W, T&&>, (int)std::is_constructible_v<X<int>, T&&>, (int)std::is_constructible_v<NoCopy, T&&>);
    printf("%d %d %d %d\n", (int)std::is_constructible_v<V, const T&>, (int)std::is_constructible_v<W, const T&>, (int)std::is_constructible_v<X<int>, const T&>, (int)std::is_constructible_v<NoCopy, const T&>);
    V v(T { });
    W w(T { });
    X<int> x(T { });
    printf("%d %d %d\n", v.x, w.x, x.x);
    std::optional<T> some { T { } };
    std::optional<T> none;
    printf("%d %d %d\n", convert(std::move(some)), convert(std::optional<T> { T { } }), convert(std::move(none)));
    return 2;
}
