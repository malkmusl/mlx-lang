// Globals pointing at a base of a global object: a base after the first
// (a constant: the object's address and the base's offset), a virtual
// base (its offset read from the vtable when the program starts), a
// member of a virtual base; a class with virtual bases given an
// alignment.
#include <stdio.h>

struct A { int a = 1; virtual int fa() { return a; } };
struct B { int b = 2; virtual int fb() { return b; } };
struct C : A, B { int c = 3; };
C gc;
B *gb = &gc;
B *gbCast = (B *)&gc;

struct V { int v = 1; virtual int f() { return v * 10; } };
struct B1 : virtual V { int b1 = 2; };
struct B2 : virtual V { int b2 = 3; };
struct D : B1, B2 { int d = 4; };
D gd;
V *gv = &gd;
B2 *gb2 = &gd;
V *gvThroughB2 = gb2;
int *gi = &gd.v;

struct alignas(32) Big : virtual V { char c; };

int main(int argc, char **argv) {
    B *local = &gc;
    printf("C: %ld %ld %ld %d\n", (char *)gb - (char *)&gc, (char *)gbCast - (char *)&gc, (char *)local - (char *)&gc, gb->fb());
    printf("D: %ld %ld %ld %d %d %d\n", (char *)gv - (char *)&gd, (char *)gb2 - (char *)&gd, (char *)gvThroughB2 - (char *)&gd, *gi, gv->f(), gb2->b2);
    printf("Big %zu %zu\n", sizeof(Big), alignof(Big));
    return argc + 1;
}
