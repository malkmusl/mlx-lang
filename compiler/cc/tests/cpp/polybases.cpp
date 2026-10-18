// Polymorphic bases after the first (a secondary vtable per such subobject, its slots adjusting this; delete through any base frees the whole object), a class's own vtable pointer over a base without virtual functions, constructors and destructors defaulted outside the class, a class template's friend functions made when used.
#include <stdio.h>

namespace p_mi1 {
struct A { virtual ~A() { printf("~A\n"); } virtual int a() { return 1; } int x = 10; };
struct B { virtual ~B() { printf("~B\n"); } virtual int b() { return 2; } virtual int bb() { return 22; } int y = 20; };
struct C : A, B { ~C() override { printf("~C\n"); } int a() override { return 3; } int b() override { return 4 + y; } int z = 30; };
struct D : C { int bb() override { return 99 + x; } };
struct N { int n = 5; };
struct E : N { virtual int e() { return n + 1; } };
struct F : N, B { int b() override { return n * 100; } };
int callB(B* p) { return p->b() * 1000 + p->bb(); }
void run() {
    C c;
    printf("%d %d %d %d\n", c.a(), c.b(), callB(&c), ((A&)c).a());
    D d;
    printf("%d %d\n", callB(&d), d.bb());
    B* pb = new C;
    printf("%d\n", pb->b());
    delete pb;
    E e; F f;
    printf("%d %d %d\n", e.e(), callB(&f), f.n);
    A* pa = new D;
    printf("%d\n", pa->a());
    delete pa;
}
}

namespace p_dd1 {
struct M { M() { printf("M\n"); } ~M() { printf("~M\n"); } };
struct X { X(); ~X(); M m; int v = 7; };
X::X() = default;
X::~X() = default;
void run() { X x; printf("%d\n", x.v); }
}

namespace p_fr1 {
enum class Eq { No, Yes };
template<typename T, Eq e = Eq::No> struct Checked {
    friend bool operator==(const Checked&, const Checked&) { static_assert(e == Eq::Yes, "opt in"); return true; }
    int v = 1;
};
template<typename T> struct Box { friend int peek(const Box& b) { return b.x * 2; } int x = 21; };
void run() { Checked<int> c; Checked<int, Eq::Yes> d, d2; Box<long> b; printf("%d %d %d\n", c.v, d == d2, peek(b)); }
}

int main() {
    p_mi1::run();
    p_dd1::run();
    p_fr1::run();
    return 0;
}
