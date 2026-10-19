// A structured binding of a class std::tuple_size is specialized for
// reads its parts through the class's member `get<i>()`, else through
// `get<i>(object)` found by argument-dependent lookup (WebCore's
// ColorComponents, SpaceSeparatedPoint, and the hidden friends of
// CSSValueAggregates.h); a part returned by value is held by the name.
#include <stdio.h>
#include <array>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace C {
template<typename T, size_t N> struct Components {
    constexpr Components() : components { } { }
    template<typename... Ts> constexpr Components(Ts... input) : components {{ input... }} { }
    template<size_t I> constexpr T get() const;
    std::array<T, N> components;
};
template<typename T, size_t N> template<size_t I> constexpr T Components<T, N>::get() const { return components[I]; }

template<typename T> struct Pair { T first; T second; };
template<size_t I, typename T> constexpr const T &get(const Pair<T> &pair) { if constexpr (!I) return pair.first; else return pair.second; }
template<typename T> struct Point { Pair<T> value; };
template<size_t I, typename T> constexpr decltype(auto) get(const Point<T> &point) { return get<I>(point.value); }

struct Named {
    std::string text;
    int number;
    std::string extra;
    template<size_t I> friend auto get(const Named &self)
    {
        if constexpr (!I) return self.text + "!";
        else return self.number * 2;
    }
};

struct Plain { int a; int b; };
}

namespace std {
template<typename T, size_t N> class tuple_size<C::Components<T, N>> : public std::integral_constant<size_t, N> { };
template<size_t I, typename T, size_t N> class tuple_element<I, C::Components<T, N>> { public: using type = T; };
template<typename T> class tuple_size<C::Point<T>> : public std::integral_constant<size_t, 2> { };
template<size_t I, typename T> class tuple_element<I, C::Point<T>> { public: using type = T; };
template<> class tuple_size<C::Named> : public std::integral_constant<size_t, 2> { };
template<size_t I> class tuple_element<I, C::Named> { public: using type = std::conditional_t<!I, std::string, int>; };
}

namespace C {
template<typename T> struct Resolved {
    static constexpr Components<float, 4> resolve(Components<float, 4> c)
    {
        auto [c1, c2, c3, alpha] = c;
        return { c1 * 2, c2 * 2, c3 * 2, alpha };
    }
};
}

int main(int argc, char **argv)
{
    C::Components<float, 4> x { 1.f, 2.f, 3.f, 4.f };
    auto [a, b, c, d] = C::Resolved<int>::resolve(x);
    printf("%g %g %g %g\n", a, b, c, d);
    C::Point<int> p { { 5, 6 } };
    const auto &[px, py] = p;
    printf("%d %d\n", px, py);
    auto [text, number] = C::Named { "name", 21, "-" };
    printf("%s %d\n", text.c_str(), number);
    auto [pa, pb] = C::Plain { 7, 8 };
    printf("%d\n", pa * 10 + pb);
    return argc;
}
