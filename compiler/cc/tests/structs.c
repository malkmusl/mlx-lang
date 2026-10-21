/* Structs and unions: layout, bit-fields, initializers, values. */
#include <stdio.h>
#include <stddef.h>

struct point { int x, y; };
struct mixed { char c; double d; short s; };
struct packed { char c; int i; } __attribute__((packed));
union number { int i; float f; unsigned char bytes[4]; };
struct flags { unsigned a : 3; unsigned b : 5; int c : 4; unsigned : 0; unsigned d : 7; };
struct anon { int kind; union { int i; double d; }; struct { int lo, hi; }; };
struct small { char a; short b; };
struct pair { long first; long second; };
struct big { long v[5]; };
struct fpair { double a; double b; };
struct mixpair { int i; float f; };
struct node { int value; struct node *next; };
struct text { int length; char data[]; };

static struct point make(int x, int y) { struct point p = { x, y }; return p; }
static struct small smallOf(char a, short b) { struct small s = { a, b }; return s; }
static struct pair swap(struct pair p) { struct pair q = { p.second, p.first }; return q; }
static struct big bigger(struct big b, int by) { for (int i = 0; i < 5; i++) b.v[i] += by; return b; }
static struct fpair scale(struct fpair p, double k) { p.a *= k; p.b *= k; return p; }
static struct mixpair twice(struct mixpair m) { m.i *= 2; m.f *= 2; return m; }
static int sum(struct node *n) { int s = 0; for (; n; n = n->next) s += n->value; return s; }

struct point origin = { 1, 2 };
struct point corners[] = { { 0, 0 }, [2] = { .y = 5, .x = 4 } };

int main(void) {
    printf("%d %d %d\n", (int)sizeof(struct point), (int)sizeof(struct mixed), (int)offsetof(struct mixed, s));
    printf("%d %d\n", (int)sizeof(struct packed), (int)offsetof(struct packed, i));
    printf("%d %d\n", (int)sizeof(union number), (int)_Alignof(struct mixed));
    union number n;
    n.f = 1.0f;
    printf("%x %d\n", n.i, n.bytes[3]);
    struct flags f = { 5, 17, -3 };
    f.d = 100;
    printf("%d %d %d %d %d\n", f.a, f.b, f.c, f.d, (int)sizeof f);
    f.a += 4;
    f.c = 7;
    printf("%d %d\n", f.a, f.c);
    struct anon an;
    an.kind = 1; an.d = 2.5; an.lo = 3; an.hi = 4;
    printf("%d %g %d %d %d\n", an.kind, an.d, an.lo, an.hi, (int)sizeof an);
    struct point p = make(3, 4);
    struct point q = p;
    q.x = 10;
    printf("%d %d %d %d\n", p.x, p.y, q.x, q.y);
    struct small s = smallOf('z', 300);
    printf("%c %d\n", s.a, s.b);
    struct pair pr = swap((struct pair){ 11, 22 });
    printf("%ld %ld\n", pr.first, pr.second);
    struct big b = { { 1, 2, 3, 4, 5 } };
    struct big c = bigger(b, 10);
    printf("%ld %ld %ld\n", b.v[0], c.v[0], c.v[4]);
    struct fpair fp = scale((struct fpair){ 1.5, -2.0 }, 3.0);
    printf("%g %g\n", fp.a, fp.b);
    struct mixpair mp = twice((struct mixpair){ 21, 1.25f });
    printf("%d %g\n", mp.i, mp.f);
    struct node n3 = { 3, 0 }, n2 = { 2, &n3 }, n1 = { 1, &n2 };
    printf("%d\n", sum(&n1));
    printf("%d %d %d %d\n", origin.x, origin.y, corners[2].x, corners[2].y);
    printf("%d\n", (int)(sizeof corners / sizeof corners[0]));
    struct point *pp = &p;
    pp->y += 100;
    (*pp).x *= 2;
    printf("%d %d\n", p.x, p.y);
    static struct text t = { 3, "abc" };
    printf("%d %s %d\n", t.length, t.data, (int)sizeof(struct text));
    struct point arr[3] = { [1].x = 7, [1].y = 8 };
    printf("%d %d %d\n", arr[0].x, arr[1].x, arr[1].y);
    struct { int a[3]; char b; } local = { { 1, 2 }, 'q' };
    printf("%d %d %d %c\n", local.a[0], local.a[1], local.a[2], local.b);
    return 0;
}
