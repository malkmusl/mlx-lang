// Probes of the WTF work (each its own namespace), compared with g++.
#include <stdio.h>

namespace mi {
struct Empty { void hello() const { printf("empty %d\n", (int)sizeof(*this)); } };
struct Shape { int kind = 1; int area() const { return kind * 10; } };
struct Counted { int count = 5; void bump() { ++count; } int value() const { return count; } };
struct NoVirtualDestructorBase { protected: NoVirtualDestructorBase() = default; };
struct Str : private Shape, public NoVirtualDestructorBase { int len = 3; int shapeArea() const { return area(); } };
struct Both : public Shape, public Counted {
    Both() : Shape(), Counted() { kind = 2; }
    int sum() const { return area() + value() + kind + count; }
};
struct WithEmpty : Empty, Counted { int z = 9; };
struct Base1 { int a; Base1(int x) : a(x) {} ~Base1() { printf("~Base1 %d\n", a); } };
struct Base2 { int b; Base2(int x) : b(x) {} ~Base2() { printf("~Base2 %d\n", b); } };
struct Derived : Base1, Base2 { int c; Derived() : Base1(1), Base2(2), c(3) {} ~Derived() { printf("~Derived\n"); } };
static int readB(const Base2 &r) { return r.b; }
static int readBp(const Base2 *p) { return p ? p->b : -1; }
void run() {
    Both b; b.bump();
    printf("%d %d %d %d\n", b.sum(), b.area(), b.value(), (int)sizeof(Both));
    Counted *c = &b; Shape *s = &b;
    printf("%d %d %d\n", c->value(), s->kind, (char *)c - (char *)&b);
    Both *back = static_cast<Both *>(c);
    printf("%d\n", back == &b);
    Str str; printf("%d %d %d\n", str.shapeArea(), str.len, (int)sizeof(Str));
    WithEmpty w; w.hello(); w.bump(); printf("%d %d %d\n", w.value(), w.z, (int)sizeof(WithEmpty));
    {
        Derived d;
        printf("%d %d %d %d %d\n", d.a, d.b, d.c, readB(d), readBp(&d));
        Base2 *nb = (Derived *)0; printf("%d\n", readBp(nb));
    }
    return;
}

}

namespace rp {
template<typename X, typename Y = int> struct Ref { X *p; X &leak() { return *p; } };
template<typename T, typename U = int, typename V = int> class RefPtr;
template<typename T, typename U, typename V>
class RefPtr {
public:
    RefPtr() : m(nullptr) {}
    template<typename X, typename Y> RefPtr(Ref<X, Y> &&);
    T *m;
};
template<typename X, typename Y> RefPtr(Ref<X, Y>&&) -> RefPtr<X, Y, int>;
template<typename T, typename U, typename V>
template<typename X, typename Y>
inline RefPtr<T, U, V>::RefPtr(Ref<X, Y>&& reference) : m(&reference.leak()) {}
void run() { int a = 4; RefPtr<int> r(Ref<int>{&a}); RefPtr q(Ref<int>{&a}); printf("%d %d\n", *r.m, *q.m); }

}

namespace rp4 {
template<typename X, typename Y = int> struct Ref { X *p; X &leak() { return *p; } };
template<typename X, typename Y> int f(Ref<X, Y> &&r) { return r.leak(); }
template<typename X, typename Y> int g(const Ref<X, Y> &r) { return *r.p; }
void run() { int a = 4; Ref<int> l{&a}; printf("%d %d %d\n", f(Ref<int>{&a}), g(l), g(Ref<int>{&a})); }

}

namespace ov {
template<typename T, unsigned N> unsigned lit(const T (&chars)[N]) { return N; }
unsigned lit(int x) { return x; }
struct H {
    template<typename T, unsigned N> static unsigned m(const T (&chars)[N]) { return N; }
    static unsigned m(int x) { return x; }
};
void run() { printf("%u %u %u %u\n", lit("abc"), lit(5), H::m("ab"), H::m(7)); }

}

namespace ra2 {
struct H {
    template<typename T, unsigned N> static constexpr unsigned lit(const T (&chars)[N]);
    static constexpr unsigned lit(int x) { return x; }
};
template<typename T, unsigned characterCount>
constexpr unsigned H::lit(const T (&characters)[characterCount]) { constexpr unsigned w = characterCount - 1; return w + characters[0]; }
void run() { printf("%u %u\n", H::lit("abc"), H::lit(5)); }

}

int main() {
    mi::run();
    rp::run();
    rp4::run();
    ov::run();
    ra2::run();
    return 0;
}
