// Function templates and deduction guides taking pointers to member
// functions (WebCore's DiscreteWrapper guides): `void (B::*)(T)` names
// template parameters, so a member pointer of another parameter type
// fits it only by deduction, not as a converting argument (`T` deduced
// `const X &` from the getter, the setter taking `X &&`, fits no
// `(T)`), and the guide of `const T & ... (T &&)` is the one chosen.
#include <cstdio>
struct X { int v; };
struct S {
    X x { 5 };
    const X& get() const { return x; }
    void set(X&& n) { x = n; }
    int i = 3;
    int geti() const { return i; }
    void seti(int n) { i = n; }
};
template<typename T, typename G = T, typename Set = T> struct W {
    W(G (S::*g)() const, void (S::*s)(Set)) : g(g), s(s) { }
    G (S::*g)() const;
    void (S::*s)(Set);
    void run(S& d, const S& from) const { (d.*s)(T { (from.*g)() }); }
};
template<typename T, typename A, typename B> W(T (A::*)() const, void (B::*)(T)) -> W<T, T, T>;
template<typename T, typename A, typename B> W(const T& (A::*)() const, void (B::*)(T&&)) -> W<T, const T&, T&&>;
template<typename T, typename A, typename B> W(T (A::*)() const, void (B::*)(T&&)) -> W<T, T, T&&>;
template<typename T, typename A, typename B> int f(T (A::*)() const, void (B::*)(T)) { return 1; }
template<typename T, typename A, typename B> int f(const T& (A::*)() const, void (B::*)(T&&)) { return 2; }
template<typename T, typename A, typename B> int f(T (A::*)() const, void (B::*)(T&&)) { return 3; }
template<typename T, typename A> int g(T (A::*)() const, void (A::*)(T)) { return 1; }
template<typename T, typename A> int g(const T& (A::*)() const, void (A::*)(T&&)) { return 2; }
template<typename T> int h(T (S::*)() const, void (S::*)(T)) { return 1; }
template<typename T> int h(const T& (S::*)() const, void (S::*)(T&&)) { return 2; }
int main()
{
    S a, b;
    b.x.v = 9;
    b.i = 4;
    W w(&S::get, &S::set);
    w.run(a, b);
    W wi(&S::geti, &S::seti);
    wi.run(a, b);
    printf("%d %d\n", a.x.v, a.i);
    printf("%d %d %d\n", f(&S::get, &S::set), g(&S::get, &S::set), h(&S::get, &S::set));
    return 0;
}
