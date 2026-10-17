// Overloads alike but for derived-to-base conversions: the nearer base wins (AtomString(UniquedStringImpl *) over AtomString(StringImpl *) for a SymbolImpl *)
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

int main() {
    p_derivrank::run();
    p_atomstr::run();
    return 0;
}
