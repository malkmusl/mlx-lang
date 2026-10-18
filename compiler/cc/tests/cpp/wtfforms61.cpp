// Constructor templates for a temporary of another instance: the one taking
// const C<U, H> & (more specialized) over the one taking U by value, as for
// WTF's Checked<int> made from a Checked<unsigned> sum; and by value as good
// as T && for rvalues (two iterators by value over (R &&, M &&), as
// EmbeddedFixedVector's).
#include <stdio.h>
struct H1 {}; struct H2 {};
template <typename T, typename OH = H1> class C : public OH {
public:
    C() : m(0) {}
    C(const C& v) : m(v.m) {}
    template <typename U> C(U value) : m((T)value) { printf("from value\n"); }
    template <typename V> C(const C<T, V>& r) : m(r.m) { printf("same T\n"); }
    template <typename U> C(const C<U, OH>& r) : m((T)r.m) { printf("same OH\n"); }
    template <typename U, typename V> C(const C<U, V>& r) : m((T)r.m) { printf("other\n"); }
    T m;
};
template <typename U, typename V> C<unsigned> operator+(C<U> a, C<V> b) { C<unsigned> r; r.m = a.m + b.m; return r; }
C<unsigned> mk() { C<unsigned> r; r.m = 9; return r; }
namespace pick {
struct It { int* p; };
struct Arr {
    template <typename I> Arr(unsigned n, I first, I last) { printf("iterators %u\n", n); }
    template <typename R, typename M> Arr(unsigned n, R&& r, M&& m) { printf("range %u\n", n); }
};
template <typename T> struct Box {
    template <typename U> Box(U&& u) { printf("forward\n"); }
    Box(const Box&) { printf("copy\n"); }
};
It mk() { return It { nullptr }; }
int run() {
    Arr a(1, It { nullptr }, It { nullptr });
    It x { nullptr };
    Arr b(2, x, x);
    Arr c(3, mk(), mk());
    Arr d(4, x, 5);
    return 0;
}
}
int main() {
    pick::run();
    C<unsigned> u; u.m = 5;
    C<int> a = u;
    C<int> b(u);
    C<int, H2> c = u;
    C<unsigned> d = u;
    C<int> e = 7;
    C<int> f = mk();
    C<int> g = u + u;
    C<int> h = C<int>(3) + u;
    printf("%d %d %d\n", f.m, g.m, h.m);
    printf("%d %d %d %u %d\n", a.m, b.m, c.m, d.m, e.m);
    return 0;
}
