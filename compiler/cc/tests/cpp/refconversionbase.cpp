// A Ref<T>'s operator T & binds a reference to a base of T, T a template's
// instance not made before (Ref<Float32Array> to ArrayBufferView &).
#include <stdio.h>
struct Buffer { };
struct View { virtual ~View() {} int kind = 1; };
template <class A> struct Generic : View { Generic() { kind = 2; } };
struct FloatAdaptor {};
using FloatView = Generic<FloatAdaptor>;
template <class T> struct Ref {
    T *p;
    Ref(T &t) : p(&t) {}
    operator T &() const { return *p; }
};
struct Global {};
struct DOMGlobal : Global {};
int f(Global *, DOMGlobal *, Buffer &) { return 1; }
int f(Global *, Global *, View &view) { return 10 + view.kind; }
int probe(Global &l, DOMGlobal &g, const Ref<FloatView> &r) { return f(&l, &g, r); }
int main() {
    FloatView fv;
    const Ref<FloatView> r(fv);
    DOMGlobal g;
    printf("%d\n", probe(g, g, r));
}
