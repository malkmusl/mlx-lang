// Calls the LIR back end lowers in place: early returns, structs returned
// in registers and passed by value, a static local, an address taken, a
// label, a switch, nested and recursive calls, the arguments' order, a
// void function. The output is the same without inlining (-fno-inline).
#include <stdio.h>
#include <string.h>
typedef struct { int x, y; } P;
typedef struct { double a; long b; } Q;
static inline int addi(int a, int b) { return a + b; }
static inline P mkp(int x, int y) { P p; p.x = x; p.y = y; return p; }
static inline int px(P p) { return p.x; }
static inline int sum(P p) { return p.x + p.y; }
static inline Q mkq(double a, long b) { Q q = { a, b }; return q; }
static inline int early(int v) { if (v < 0) return -1; if (v == 0) return 0; return 1; }
static inline int counter(void) { static int n; return ++n; }
static inline int addr(int v) { int local = v; int *p = &local; *p += 1; return local; }
static inline unsigned char narrow(int v) { return (unsigned char)v; }
static inline long lbl(int n) { long t = 0; int i = 0; again: t += i; i++; if (i < n) goto again; return t; }
static inline int sw(int k) { switch (k) { case 1: return 10; case 2: return 20; default: return 30; } }
static inline int nested(int a) { return addi(early(a), sw(a)); }
static int order(int tag) { printf("<%d>", tag); return tag; }
static inline int two(int a, int b) { return a * 10 + b; }
static inline double fl(float f, double d) { return f + d; }
static inline void voidf(int *p) { if (*p > 3) return; *p = 99; }
static inline int rec(int n) { return n <= 0 ? 0 : n + rec(n - 1); }
static inline int arr(int n) { int a[4] = { 1, 2, 3, 4 }; return a[n & 3] + n; }
static inline const char *str(int k) { return k ? "yes" : "no"; }
int main(void) {
    P p = mkp(3, 4);
    Q q = mkq(1.5, 7);
    int v = 5;
    voidf(&v);
    int w = 1;
    voidf(&w);
    printf("%d %d %d %d %d %g %ld\n", addi(1, 2), px(p), sum(p), sum(mkp(10, 20)), px(mkp(7, 8)), q.a, q.b);
    printf("%d %d %d %d %d %d\n", early(-5), early(0), early(9), counter(), counter(), counter());
    printf("%d %u %ld %d %d %d\n", addr(41), narrow(300), lbl(5), sw(1), sw(2), sw(5));
    printf("%d %d %d\n", nested(-3), nested(1), nested(2));
    printf("%d\n", two(order(1), order(2)));
    printf("%g %d %d %d %d %s %s\n", fl(1.25f, 2.0), v, w, rec(10), arr(6), str(0), str(1));
    char buf[16]; strcpy(buf, str(1)); printf("%zu\n", strlen(buf));
    return 0;
}
