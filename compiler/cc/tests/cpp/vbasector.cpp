// Construction and destruction with virtual bases: a virtual base is
// made once, by the most derived class, from that class's initializer
// (the intermediate classes' ones are not used then), before the other
// bases, and unmade last; while a base is being made or unmade its
// virtual functions are its own and its virtual bases where the whole
// object has them (construction vtables).
#include <stdio.h>

struct V {
    int v;
    V() : v(-1) { printf("V() %d\n", v); }
    V(int x) : v(x) { printf("V(%d)\n", x); }
    virtual ~V() { printf("~V %d\n", v); }
    virtual const char *name() const { return "V"; }
    void show(const char *where) const { printf("%s: name %s v %d\n", where, name(), v); }
};

struct B1 : virtual V {
    int b1;
    B1(int x) : V(x * 10), b1(x) { printf("B1(%d) v %d\n", x, v); show("in B1()"); }
    ~B1() { printf("~B1 %d\n", b1); show("in ~B1"); }
    const char *name() const override { return "B1"; }
};

struct B2 : virtual V {
    int b2;
    B2(int x) : V(x * 100), b2(x) { printf("B2(%d) v %d\n", x, v); show("in B2()"); }
    ~B2() { printf("~B2 %d\n", b2); }
};

struct D : B1, B2 {
    int d;
    D() : V(7), B1(1), B2(2), d(3) { printf("D() v %d\n", v); show("in D()"); }
    D(int x) : B1(x), B2(x + 1), d(x + 2) { printf("D(%d) v %d\n", x, v); }
    ~D() { printf("~D %d\n", d); }
    const char *name() const override { return "D"; }
};

// A class whose virtual base is made through two levels, initialized by
// the most derived class only (an indirect virtual base).
struct Top : D {
    Top() : V(99), D() { printf("Top() v %d\n", v); }
    ~Top() { printf("~Top\n"); }
};

// Virtual bases with members of class type and implicit constructors.
struct Member {
    int id;
    Member(int i) : id(i) { printf("Member(%d)\n", i); }
    ~Member() { printf("~Member(%d)\n", id); }
};
struct W { Member wm{1}; W() { printf("W()\n"); } virtual ~W() { printf("~W\n"); } };
struct X1 : virtual W { Member xm{2}; };
struct X2 : virtual W { Member ym{3}; X2() { printf("X2()\n"); } };
struct XD : X1, X2 { Member zm{4}; };

int main(int argc, char **argv) {
    {
        printf("-- D\n");
        D d;
        d.show("main");
        printf("sizes %zu %zu %zu\n", sizeof(V), sizeof(B1), sizeof(D));
    }
    {
        printf("-- D(5)\n");
        D d(5);
        d.show("main");
    }
    {
        printf("-- B1 alone\n");
        B1 b(4);
        b.show("main");
    }
    {
        printf("-- Top\n");
        Top t;
        t.show("main");
    }
    {
        printf("-- delete through V\n");
        V *p = new D(8);
        p->show("through V");
        delete p;
    }
    {
        printf("-- XD\n");
        XD x;
        printf("members %d %d %d %d\n", x.wm.id, x.xm.id, x.ym.id, x.zm.id);
    }
    {
        printf("-- XD through W\n");
        W *w = new XD;
        delete w;
    }
    return argc + 1;
}
