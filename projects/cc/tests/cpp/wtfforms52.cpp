// Overloads alike but for derived-to-base conversions: the nearer base wins (AtomString(UniquedStringImpl *) over AtomString(StringImpl *) for a SymbolImpl *), a base pointer over void *; promotions, conversions, templates
#include <stdio.h>

namespace p_derivrank {
struct A { int a = 1; };
struct B : A { int b = 2; };
struct C : B { int c = 3; };
struct D : C {};
static int f(A *) { return 1; }
static int f(B *) { return 2; }
static int g(A &) { return 1; }
static int g(C &) { return 3; }
static int h(const A &) { return 1; }
static int h(const B &) { return 2; }
struct S { int which; S(A *) : which(1) {} S(B *) : which(2) {} };
struct T { int which; T(A *) : which(1) {} T(C *) : which(3) {} };
int run() {
    C c; D d;
    S s(&c); T t(&d);
    S s2 = &c;
    printf("%d %d %d %d %d %d %d\n", f(&c), g(d), h(c), s.which, t.which, s2.which, f(&d));
    return 0;
}
}

namespace p_atomstr {
struct StringImpl { int kind; };
struct UniquedStringImpl : StringImpl {};
struct SymbolImpl : UniquedStringImpl {};
struct AtomString {
    int how;
    AtomString(StringImpl *) : how(1) {}
    AtomString(UniquedStringImpl *) : how(2) {}
};
struct Identifier {
    AtomString m_string;
    Identifier(SymbolImpl &uid) : m_string(&uid) {}
    static Identifier fromUid(SymbolImpl &symbol) { return symbol; }
};
int run() {
    static SymbolImpl sym;
    Identifier id = Identifier::fromUid(sym);
    AtomString direct = &sym;
    printf("%d %d\n", id.m_string.how, direct.how);
    return 0;
}
}

namespace p_ovl {
struct Base2 {}; struct Derived2 : Base2 {};
static int a(long) { return 1; } static int a(double) { return 2; }
static int b(void *) { return 1; } static int b(bool) { return 2; }
static int c(Base2 *) { return 1; } static int c(void *) { return 2; }
static int d(int &) { return 1; } static int d(const int &) { return 2; }
static int e(const char *) { return 1; } static int e(bool) { return 2; }
template <class T> static int f(T) { return 1; } static int f(int) { return 2; }
template <class T> static int g(T) { return 1; } template <class T> static int g(T *) { return 2; }
static int h(int) { return 1; } static int h(long) { return 2; }
static int i(char) { return 1; } static int i(int) { return 2; }
static int j(float) { return 1; } static int j(double) { return 2; }
static int k(const Base2 &) { return 1; } static int k(const void *) { return 2; }
static int l(Base2 &) { return 1; } static int l(Derived2 &) { return 2; }
static int m(unsigned) { return 1; } static int m(long long) { return 2; }
int run() {
    int x = 0; const int cx = 0; Derived2 dd; int arr[2]; short s = 1; float fl = 1;
    printf("%d %d %d %d%d %d %d %d %d %d %d %d %d %d %d\n", a(1.0f), b(&x), c(&dd), d(x), d(cx), e("s"), f(1), g(&x), h(s), i('c'), j(fl), k(dd), l(dd), m(5u), b(arr));
    return 0;
}
}

int main() {
    p_derivrank::run();
    p_atomstr::run();
    p_ovl::run();
    return 0;
}
