// Copies and moves of classes with virtual bases: the implicit copy
// constructor copies the virtual base once (the most derived class's
// copy makes it), user-written copy constructors name it, a base part
// copied from a more derived object finds its virtual base where that
// object has it; objects passed and returned by value, assigned,
// slices assigned through a base reference keep their vtables.
#include <stdio.h>
#include <string.h>

static int copies = 0;

struct Name {
    char text[16];
    Name(const char *t) { strcpy(text, t); printf("Name(%s)\n", t); }
    Name(const Name &o) { strcpy(text, o.text); copies++; printf("Name copy %s\n", text); }
    Name &operator=(const Name &o) { strcpy(text, o.text); printf("Name assign %s\n", text); return *this; }
    ~Name() { printf("~Name %s\n", text); }
};

struct V {
    Name name{"v"};
    int v = 1;
    virtual int id() const { return v; }
    virtual ~V() {}
};

struct A : virtual V { int a = 2; int id() const override { return v * 100 + a; } };
struct B : virtual V { int b = 3; };
struct D : A, B { int d = 4; };

// A user-written copy constructor naming the virtual base.
struct E : virtual V {
    int e = 5;
    E() {}
    E(const E &o) : V(o), e(o.e + 1) { printf("E copy\n"); }
};
struct F : E { int f = 6; };

D make(int k) {
    D d;
    d.v = k;
    d.a = k + 1;
    return d;
}

int sum(D d) { return d.v + d.a + d.b + d.d + d.id(); }

int main(int argc, char **argv) {
    D one;
    one.v = 10;
    one.a = 20;
    one.b = 30;
    one.d = 40;
    printf("-- copy\n");
    D two = one;
    printf("two %d %d %d %d id %d name %s copies %d\n", two.v, two.a, two.b, two.d, two.id(), two.name.text, copies);
    printf("-- by value\n");
    int total = sum(two);
    printf("sum %d\n", total);
    printf("-- returned\n");
    D three = make(7);
    printf("three %d %d id %d\n", three.v, three.a, three.id());
    printf("-- assigned\n");
    three = one;
    printf("three %d %d %d %d id %d\n", three.v, three.a, three.b, three.d, three.id());
    printf("-- slice assigned\n");
    A lone;
    lone.v = 55;
    A &part = three;
    part = lone;
    printf("three %d %d id %d, through A %d\n", three.v, three.a, three.id(), part.id());
    printf("-- base part copied from a derived object\n");
    A copyOfPart(three);
    printf("copy of part %d %d id %d\n", copyOfPart.v, copyOfPart.a, copyOfPart.id());
    printf("-- user copy constructor\n");
    F f1;
    f1.v = 77;
    F f2 = f1;
    printf("f2 %d %d %d\n", f2.v, f2.e, f2.f);
    E e2 = f1;
    printf("e2 %d %d\n", e2.v, e2.e);
    printf("-- end\n");
    return argc + 1;
}
