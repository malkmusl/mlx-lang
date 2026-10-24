// Virtual bases reached through pointers and references: conversions to
// a virtual base read its offset from the vtable (the same object
// reached from every path), members and member functions of virtual
// bases, virtual functions overridden along one path called through
// another (the final overrider, `this` adjusted), null pointers kept
// null, objects whose dynamic type differs from the static one.
#include <stdio.h>

struct V {
    int v = 1;
    virtual int f() const { return 100 + v; }
    virtual int g() const { return 200 + v; }
    int plain() const { return v * 2; }
    virtual ~V() {}
};

struct L : virtual V {
    int l = 10;
    int f() const override { return 300 + l; }
    virtual int h() const { return 400 + l; }
};

struct R : virtual V {
    int r = 20;
    int g() const override { return 500 + r; }
    virtual int k() const { return 600 + r; }
};

// L overrides f and R overrides g: through each path the one final
// overrider.
struct J : L, R {
    int j = 30;
    int h() const override { return 700 + j + l + r; }
};

// A further class adding its own virtual base on top.
struct Extra { long e = 5; virtual long ex() const { return e; } virtual ~Extra() {} };
struct K : virtual Extra, J {
    int kk = 40;
    int f() const override { return 800 + kk; }
    long ex() const override { return 900 + kk + e; }
};

int viaV(const V &x) { return x.f() * 1000 + x.g(); }
int viaL(const L &x) { return x.f() * 1000 + x.h(); }
int viaR(const R *x) { return x->g() * 1000 + x->k(); }
int vOf(V *p) { return p ? p->v : -1; }
int plainOf(const R &x) { return x.plain(); }

V *asV(L *p) { return p; }

void show(const char *name, const J &x) {
    const L &l = x;
    const R &r = x;
    const V &v1 = l;
    const V &v2 = r;
    printf("%s: same V %d, f %d %d %d g %d %d %d\n", name, &v1 == &v2, x.f(), l.f(), v2.f(), x.g(), l.g(), r.g());
    printf("%s: h %d k %d viaV %d viaL %d viaR %d plain %d\n", name, x.h(), x.k(), viaV(x), viaL(x), viaR(&x), plainOf(x));
}

int main(int argc, char **argv) {
    J j;
    show("J", j);
    j.v = 2;
    j.l = 11;
    j.r = 21;
    show("J changed", j);

    K k;
    show("K", k);
    Extra *e = &k;
    V *v = &k;
    printf("K: ex %ld %ld f %d g %d v %d\n", e->ex(), k.ex(), v->f(), v->g(), k.v);
    printf("K: offsets J %ld V %ld Extra %ld\n", (char *)static_cast<J *>(&k) - (char *)&k, (char *)static_cast<V *>(&k) - (char *)&k, (char *)static_cast<Extra *>(&k) - (char *)&k);

    // Through a pointer to an intermediate class, whose virtual base is
    // elsewhere in J than in an L alone.
    L alone;
    L *pl = &alone;
    printf("L alone: vOf %d f %d g %d\n", vOf(pl), pl->f(), pl->g());
    pl = &j;
    printf("L in J: vOf %d f %d g %d\n", vOf(pl), pl->f(), pl->g());
    pl = 0;
    printf("null: vOf %d %d\n", vOf(pl), asV(pl) == 0);

    // Members of the virtual base through every way of naming them.
    J *pj = &j;
    pj->v = 7;
    R *pr = pj;
    pr->v += 1;
    L &rl = j;
    rl.v *= 3;
    printf("v %d %d %d %d\n", j.v, pj->V::v, static_cast<V &>(rl).v, pr->plain());

    // An array of objects with virtual bases.
    J many[3];
    for (int i = 0; i < 3; i++) many[i].v = i * 5;
    int total = 0;
    for (J &x : many) total += static_cast<R &>(x).g() + vOf(&x);
    printf("array total %d sizes %zu %zu %zu %zu\n", total, sizeof(L), sizeof(R), sizeof(J), sizeof(K));

    // Deleting through a virtual base.
    V *heap = new K;
    printf("heap f %d\n", heap->f());
    delete heap;
    return argc + 1;
}
