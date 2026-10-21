// More constructors of classes with virtual bases: delegating ones (the
// constructor delegated to makes the virtual base for a complete object
// only), constructor templates, inherited constructors, a copy
// constructor defaulted outside the class, a base with virtual bases
// initialized from braces.
#include <stdio.h>

struct V {
    int v;
    V(int x = 0) : v(x) { printf("V(%d)\n", x); }
    V(const V &o) : v(o.v) { printf("V copy %d\n", v); }
    virtual ~V() {}
};

struct Del : virtual V {
    int d;
    Del(int x) : V(x * 2), d(x) { printf("Del(%d)\n", x); }
    Del() : Del(5) { printf("Del()\n"); }
    template <class T> Del(T t, int extra) : V((int)t + extra), d(extra) { printf("Del template %d\n", d); }
};

struct Outer : Del {
    Outer() : V(100), Del() { printf("Outer() v %d d %d\n", v, d); }
    Outer(double t) : V(200), Del(t, 3) { printf("Outer(double) v %d d %d\n", v, d); }
};

struct Inherits : Del {
    using Del::Del;
};

struct Copied : virtual V {
    int c = 7;
    Copied() : V(9) {}
    Copied(const Copied &);
};
Copied::Copied(const Copied &) = default;

// A virtual base's initializer making a temporary: made and unmade by
// the complete object only.
struct Temp {
    int t;
    Temp(int x) : t(x) { printf("Temp(%d)\n", x); }
    ~Temp() { printf("~Temp %d\n", t); }
    operator int() const { return t; }
};
struct UsesTemp : virtual V {
    UsesTemp() : V(Temp(41)) { printf("UsesTemp v %d\n", v); }
};
struct AroundTemp : UsesTemp {
    AroundTemp() : V(Temp(42)) { printf("AroundTemp v %d\n", v); }
};

struct Agg : virtual V { int a; int b; Agg(int x, int y) : V(x + y), a(x), b(y) {} };
struct HoldsAgg : Agg { HoldsAgg() : V(1), Agg{2, 3} {} };

int main(int argc, char **argv) {
    printf("-- Del()\n");
    Del d;
    printf("v %d d %d\n", d.v, d.d);
    printf("-- Outer()\n");
    Outer o;
    printf("-- Del template\n");
    Del t(2.5, 4);
    printf("v %d d %d\n", t.v, t.d);
    printf("-- Outer(double)\n");
    Outer od(1.5);
    printf("-- Inherits\n");
    Inherits in(6);
    printf("v %d d %d\n", in.v, in.d);
    printf("-- Copied\n");
    Copied c1;
    c1.v = 11;
    Copied c2 = c1;
    printf("copied %d %d\n", c2.v, c2.c);
    printf("-- UsesTemp\n");
    UsesTemp ut;
    printf("-- AroundTemp\n");
    AroundTemp at;
    printf("-- HoldsAgg\n");
    HoldsAgg h;
    printf("agg %d %d %d\n", h.v, h.a, h.b);
    return argc + 1;
}
