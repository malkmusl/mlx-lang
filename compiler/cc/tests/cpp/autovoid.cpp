// `auto` result types with no value returned are void, in decltype and
// std::is_invocable too: a generic lambda that returns nothing visits a
// variant's alternatives (WTF's Variant checks the visitor with
// is_invocable, WebCore's ColorInterpolationMethod hashes so).
#include <cstdio>
#include <type_traits>
#include <utility>

template<class A, class... B> struct Visitor : Visitor<A>, Visitor<B...> {
    Visitor(A a, B... b) : Visitor<A>(a), Visitor<B...>(b...) { }
    using Visitor<A>::operator ();
    using Visitor<B...>::operator ();
};
template<class A> struct Visitor<A> : A {
    Visitor(A a) : A(a) { }
    using A::operator();
};
template<class... F> Visitor<F...> makeVisitor(F... f) { return Visitor<F...>(f...); }

struct HSL { static constexpr int space = 1; int hue = 3; };
struct Lab { static constexpr int space = 2; };

struct Hasher { int value = 0; };
inline void add(Hasher& h, int v) { h.value = h.value * 10 + v; }

template<typename V, typename... Values> struct check {
    static_assert(std::is_invocable<V, Values...>::value, "`visit` requires the visitor to be exhaustive.");
    static constexpr bool ok = true;
};

template<typename T> auto nothing(const T& x) { (void)x; }
static_assert(std::is_same_v<decltype(nothing(std::declval<HSL&>())), void>);

int main()
{
    Hasher hasher;
    auto lambda = [&]<typename M>(const M& m) {
        add(hasher, M::space);
        if constexpr (M::space == 1)
            add(hasher, m.hue);
    };
    auto visitor = makeVisitor(lambda, [](int) { });
    static_assert(check<decltype(visitor), HSL&>::ok);
    static_assert(check<decltype(visitor), const Lab&>::ok);
    static_assert(check<decltype(visitor), int>::ok);
    visitor(HSL { });
    visitor(Lab { });
    visitor(5);
    std::printf("%d\n", hasher.value);
    return 0;
}
