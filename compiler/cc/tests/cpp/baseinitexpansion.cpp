// A constructor's initializer expanding a pack of bases, `: F(f)...`
// (an overload set of lambdas, as WTF's visitors make it): one
// initializer per base, each from its element of the parameter pack;
// none for an empty pack, whose base clause `: F...` names no base.
#include <stdio.h>
#include <utility>

template<class... F> struct Overloads : F... {
    Overloads(F... f) : F(f)... { }
    using F::operator()...;
};
template<class... F> struct Tagged : F... {
    int tag;
    Tagged(int t, F&&... f) : F(std::move(f))..., tag(t) { }
};
template<class... F> Overloads<F...> overloads(F... f) { return Overloads<F...>(f...); }

struct A { int a; A(int x) : a(x) { printf("A %d\n", x); } };
struct B { int b; B(int x) : b(x) { printf("B %d\n", x); } };

int main(int argc, char **)
{
    auto o = overloads([](int x) { return x + 1; }, [](const char *) { return 100; }, [](double d) { return (int)(d * 10); });
    printf("%d %d %d\n", o(4), o("x"), o(2.5));
    A a(1);
    B b(2);
    Tagged<A, B> t(7, std::move(a), std::move(b));
    printf("%d %d %d\n", t.tag, t.a, t.b);
    Tagged<> none(3);
    printf("%d\n", none.tag);
    return argc;
}
