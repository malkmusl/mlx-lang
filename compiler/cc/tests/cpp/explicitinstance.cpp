// Explicit instantiation definitions make what they name though nothing
// in the file calls it (tools/check_cpp.sh looks at the object's names):
// a member function template's instance of a plain class, a function
// template's (its arguments given or deduced), and every member of a
// class template's instance (its static data members and member
// classes' members too; not its member templates), weak as g++ makes
// them. `extern template` makes nothing.
#include <cstdio>

struct V1 { int n = 1; };
struct V2 { int n = 2; };
struct S {
    template<typename V> void visit(V&);
    int total = 0;
};
template<typename V> void S::visit(V& v) { total += v.n; }
template void S::visit(V1&);
template void S::visit<V2>(V2&);

template<int N> struct C {
    int f();
    int g() { return N * 10; }
    static int s;
    struct Inner { int h() { return N + 1000; } };
    template<class T> T same(T t) { return t; }
};
template<int N> int C<N>::f() { return N; }
template<int N> int C<N>::s = N + 100;
template struct C<3>;
extern template struct C<4>;

template<class T> T twice(T t) { return t + t; }
template int twice(int);
template double twice<double>(double);
extern template long twice(long);

int main()
{
    S s;
    V1 v1;
    s.visit(v1);
    std::printf("%d %d %d\n", s.total, C<3>::s, twice(4));
    return 0;
}
