// Value-initialization: new T(), new T[n](), new (Tag(), where) T() (a temporary
// among the placement arguments), m() and arr() in a constructor's list, and
// array elements left out of braces zero the object before its default member
// initializers.
#include <stdio.h>
#include <string.h>
#include <new>
struct P { char c; int i; double d { 1.5 }; };
struct Q { int a; int b = 7; };
struct NotNullTag {};
inline void* operator new(size_t, NotNullTag, void* p) { return p; }
template <class T> void init(T* b, T* e) { for (T* c = b; c != e; ++c) new (NotNullTag(), c) T(); }
template <class T> void init2(T* b, T* e) { for (T* c = b; c != e; ++c) new (c) T(); }
int vinit2() {
    int* a = new int(); int* b = new int[4](); Q* c = new Q[3](); P* d = new P{}; 
    printf("%d %d %d %d %d %d %d %g\n", *a, b[0], b[3], c[2].a, c[2].b, d->c, d->i, d->d);
    return 0;
}
namespace v2 {
struct P { char c; int i; double d { 1.5 }; };
struct H { int a; P p; int arr[3]; H() : a(), p(), arr() {} };
struct K { int x; int y = 2; };
template <class T, class... A> T* mk(A... a) { return new T(a...); }
int run() {
    alignas(16) unsigned char buf[128];
    memset(buf, 0xab, sizeof buf); H* h = new (buf) H; printf("%d %d %d %g %d %d\n", h->a, h->p.c, h->p.i, h->p.d, h->arr[0], h->arr[2]);
    memset(buf, 0xab, sizeof buf); K* k = new (buf) K{}; printf("%d %d\n", k->x, k->y);
    K* k2 = mk<K>(); printf("%d %d\n", k2->x, k2->y);
    int* i = mk<int>(); printf("%d\n", *i);
    memset(buf, 0xab, sizeof buf); { K* kk = (K*)buf; *kk = K(); printf("%d %d\n", kk->x, kk->y); }
    K arr[2] = {}; printf("%d %d\n", arr[1].x, arr[1].y);
    K one {}; printf("%d %d\n", one.x, one.y);
    return 0;
}
}
int main() {
    v2::run();
    vinit2();
    alignas(8) unsigned char buf[64]; memset(buf, 0xab, sizeof buf);
    P* p = (P*)buf; init(p, p + 2);
    printf("%d %d %g %d %d %g\n", p[0].c, p[0].i, p[0].d, p[1].c, p[1].i, p[1].d);
    memset(buf, 0xab, sizeof buf);
    init2(p, p + 2);
    printf("%d %d %g\n", p[1].c, p[1].i, p[1].d);
    memset(buf, 0xab, sizeof buf);
    Q* q = (Q*)buf; init(q, q + 2); printf("%d %d %d %d\n", q[0].a, q[0].b, q[1].a, q[1].b);
    memset(buf, 0xab, sizeof buf);
    new (q) Q(); printf("%d %d\n", q->a, q->b);
    memset(buf, 0xab, sizeof buf);
    new (q) Q{}; printf("%d %d\n", q->a, q->b);
    memset(buf, 0xab, sizeof buf);
    new (q) Q; printf("%d\n", q->b);
    Q l = Q(); printf("%d %d\n", l.a, l.b);
    return 0;
}
