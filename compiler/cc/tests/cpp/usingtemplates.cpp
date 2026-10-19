// Member function templates of one name brought from several bases by
// using-declarations are one overload set: `using B1::f; using B2::f;`,
// `using Bases::operator()...;` of a pack of bases (WTF's
// VariantBestMatch, whose result named void before), nested (a visitor
// of generic lambdas), each called on its own base.
#include <stdio.h>
#include <type_traits>
#include <utility>

struct A { int x; };

template<typename T, int I> struct Overload {
    int tag = I * 10;
    template<typename U> auto operator()(T, U &&) const -> T { return T(); }
    template<typename U> int f(T, U) const { return tag + I; }
};
template<typename... Bases> struct All : Bases... {
    void operator()() const;
    using Bases::operator()...;
    using Bases::f...;
};
template<typename B1, typename B2> struct Two : B1, B2 {
    void operator()() const;
    using B1::operator();
    using B2::operator();
};

template<typename T> const char *name() { return std::is_same_v<T, A> ? "A" : std::is_same_v<T, int> ? "int" : std::is_same_v<T, double> ? "double" : std::is_void_v<T> ? "void" : "?"; }

template<class First, class... Rest> struct Visitor : Visitor<First>, Visitor<Rest...> {
    Visitor(First first, Rest... rest) : Visitor<First>(first), Visitor<Rest...>(rest...) { }
    using Visitor<First>::operator();
    using Visitor<Rest...>::operator();
};
template<class F> struct Visitor<F> : F {
    Visitor(F f) : F(f) { }
    using F::operator();
};
template<class... F> Visitor<F...> makeVisitor(F... f) { return Visitor<F...>(f...); }

struct Line { int a; };
struct Quad { int a; int b; };
struct Tri { int a; int b; int c; };

int main(int argc, char **argv)
{
    printf("%s\n", name<decltype(std::declval<All<Overload<A, 0>, Overload<int, 1>>>()(1, 1))>());
    printf("%s\n", name<decltype(std::declval<All<Overload<A, 0>, Overload<int, 1>, Overload<double, 2>>>()(A { }, 1))>());
    printf("%s\n", name<decltype(std::declval<Two<Overload<A, 0>, Overload<int, 1>>>()(1, 1))>());
    printf("%s\n", name<std::invoke_result_t<All<Overload<A, 0>, Overload<int, 1>>, int, int>>());

    All<Overload<A, 1>, Overload<int, 2>, Overload<Line, 3>> all;
    printf("%d %d %d\n", all.f(A { }, 0), all.f(5, 0), all.f(Line { 1 }, 0));

    int base = 100 * argc;
    auto v = makeVisitor([base](Line s) { return base + s.a; }, [base](const auto &q) requires std::is_class_v<std::remove_cvref_t<decltype(q)>> { return base + q.a + q.b; }, [base](Tri t) { return base + t.a + t.b + t.c; }, [base](auto i) requires std::is_integral_v<decltype(i)> { return base * 2 + (int)i; });
    printf("%d %d %d %d\n", v(Line { 1 }), v(Quad { 2, 3 }), v(Tri { 4, 5, 6 }), v(7L));
    return argc;
}
