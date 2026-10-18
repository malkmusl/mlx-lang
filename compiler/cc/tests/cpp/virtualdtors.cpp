// Virtual destructors defaulted (`virtual ~A() = default;`) or made
// implicitly in a class whose base has a virtual one: deleting through
// the base unmakes the whole object; a destructor calls its own class's
// virtual functions (the vtable pointers set back to its class's), not
// the derived class's, as the derived part is gone already.
#include <stdio.h>

struct M { int i; ~M() { printf("~M %d\n", i); } };
struct A {
    virtual ~A() = default;
    virtual const char *name() const { return "A"; }
};
struct B : A { M m{1}; ~B() { printf("~B\n"); } };
struct C : A { M m{2}; };
struct E {
    virtual ~E() { printf("~E as %s\n", name()); }
    virtual const char *name() const { return "E"; }
};
struct F : E { M m{3}; const char *name() const override { return "F"; } };
struct G : F { M m{4}; ~G() { printf("~G as %s\n", name()); } const char *name() const override { return "G"; } };

int main(int argc, char **argv) {
    A *a = new B;
    delete a;
    A *c = new C;
    delete c;
    E *f = new F;
    delete f;
    E *g = new G;
    printf("made %s\n", g->name());
    delete g;
    {
        G local;
    }
    return argc + 1;
}
