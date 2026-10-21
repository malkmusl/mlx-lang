// Functions of one name brought from several bases by using-declarations
// are one overload set (each called on its own base): `using C::f; using
// D::f;`, nested, and WTF's Visitor of lambdas, `using A::operator();`
// beside a generic lambda's call operator template (WTF::switchOn).
#include <cstdio>
#include <utility>

struct A { int a = 1; int f(char) const { return a; } };
struct B { int b = 2; int f(short) const { return b; } };
struct C { int c = 3; int f(long) const { return c; } };
struct D { int d = 4; int f(double) const { return d; } };

struct CD : C, D { using C::f; using D::f; };
struct BCD : B, CD { using B::f; using CD::f; };
struct ABCD : A, BCD { using A::f; using BCD::f; };

template<class First, class... Rest> struct Visitor : Visitor<First>, Visitor<Rest...> {
    Visitor(First first, Rest... rest) : Visitor<First>(first), Visitor<Rest...>(rest...) { }
    using Visitor<First>::operator ();
    using Visitor<Rest...>::operator ();
};
template<class F> struct Visitor<F> : F {
    Visitor(F f) : F(f) { }
    using F::operator();
};
template<class... F> Visitor<F...> makeVisitor(F... f) { return Visitor<F...>(f...); }

struct Line { int a; int b; };
struct Quad { int a; int b; int c; };

int main()
{
    ABCD x;
    std::printf("%d %d %d %d\n", x.f('a'), x.f(short(1)), x.f(1L), x.f(1.0));
    const BCD& y = x;
    std::printf("%d %d %d\n", y.f(short(1)), y.f(1L), y.f(1.0));

    int total = 0;
    int other = 0;
    auto v = makeVisitor([&](Line s) { total += s.a; }, [&](Quad s) { other += s.a + s.b + s.c; }, [&](int s) { total += s * 1000; });
    v(Quad { 4, 5, 6 });
    v(Line { 1, 2 });
    v(3);
    std::printf("%d %d\n", total, other);
    auto w = makeVisitor([&](auto&& s) { total += 100; (void)s; }, [&](Line s) { total += s.a; }, [&](Quad s) { other += s.a + s.b + s.c; });
    w(Quad { 4, 5, 6 });
    w(Line { 1, 2 });
    w(7);
    Quad q { 1, 1, 1 };
    w(q);
    std::printf("%d %d\n", total, other);
    return total % 100;
}
