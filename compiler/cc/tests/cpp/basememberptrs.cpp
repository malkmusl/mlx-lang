// Pointers to members of a base applied to a derived object: the base
// after the first, a virtual base (the object's part of the member's
// class, then its vtable for a virtual function).
#include <stdio.h>

struct A { int a = 1; virtual int fa() { return a; } };
struct B { int b = 2; virtual int fb() { return b * 10; } int plain() { return b * 100; } };
struct C : A, B { int c = 3; int fb() override { return c * 1000; } };

struct V { int v = 4; virtual int fv() { return v; } };
struct L : virtual V { int l = 5; };
struct R : virtual V { int r = 6; int fv() override { return r * 7; } };
struct D : L, R {};

int main(int argc, char **argv) {
    C c;
    int (B::*virt)() = &B::fb;
    int (B::*nonVirt)() = &B::plain;
    int B::*field = &B::b;
    printf("C: %d %d %d\n", (c.*virt)(), (c.*nonVirt)(), c.*field);
    C *pc = &c;
    printf("C via pointer: %d %d\n", (pc->*virt)(), pc->*field);

    D d;
    int (V::*fv)() = &V::fv;
    printf("D: %d\n", (d.*fv)());
    L *pl = &d;
    printf("D via L: %d\n", (pl->*fv)());
    return argc + 1;
}
