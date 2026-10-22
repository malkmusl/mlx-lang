// A conditional of an lvalue and a const lvalue of one class is a const
// lvalue: a `T &` deduces a const T from it (`const Ref targetOrigin =
// localDocument ? localDocument->securityOrigin() :
// remoteFrame->frameDocumentSecurityOriginOrOpaque();` in WebCore's
// DOMWindow, CheckedRef of a TextRun in TextPainter).
#include <cstdio>
template<typename T> struct Ref {
    Ref(T& object) : p(&object) { }
    Ref(const Ref& o) : p(o.p) { }
    template<typename X> Ref(const Ref<X>& o) : p(o.p) { }
    T* p;
};
struct Origin { int v; };
struct Doc { Origin o { 4 }; Origin& origin() const { return const_cast<Origin&>(o); } };
struct Frame { Origin o { 5 }; const Origin& origin() const { return o; } };
template<typename T> int f(T& x) { return x.v; }
int g(Origin&) { return 1; }
int g(const Origin&) { return 2; }
int main(int argc, char**) {
    Doc d;
    Frame f0;
    Doc* dp = argc > 5 ? &d : nullptr;
    const Ref target = dp ? dp->origin() : f0.origin();
    printf("%d\n", target.p->v);
    Origin a { 6 };
    const Origin& b = a;
    Ref r2 = argc > 5 ? a : b;
    printf("%d %d\n", r2.p->v, r2.p == &a);
    printf("%d %d\n", f(argc > 5 ? a : b), g(argc > 5 ? b : a));
    a.v = 7;
    printf("%d\n", (argc > 5 ? b : a).v);
}
