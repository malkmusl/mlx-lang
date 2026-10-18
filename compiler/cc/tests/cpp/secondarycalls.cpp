// A virtual function of a base after the first called on the derived
// object: through that base's own vtable pointer (its slots are not the
// derived class's), overridden or not.
#include <stdio.h>

struct A { int a = 1; virtual int fa() { return 10 + a; } };
struct B { int b = 2; virtual int fb() { return 20 + b; } virtual int gb() const { return 30 + b; } };
struct C : A, B { int c = 3; int gb() const override { return 40 + c; } };
struct D : C { int d = 4; int fb() override { return 50 + d; } };

int main(int argc, char **argv) {
    C c;
    printf("%d %d %d\n", c.fa(), c.fb(), c.gb());
    C *p = &c;
    const C &r = c;
    printf("%d %d %d\n", p->fa(), p->fb(), r.gb());
    D d;
    B &b = d;
    printf("%d %d %d %d %d\n", d.fa(), d.fb(), d.gb(), b.fb(), b.gb());
    return argc + 1;
}
