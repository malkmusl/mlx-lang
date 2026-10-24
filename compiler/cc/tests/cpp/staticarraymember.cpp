// A static array member of a class template defined outside the class
// (`template <int N> const int Holder<N>::values[N] = { ... };`) is
// defined for each instance: read as a variable template named
// `values`, no instance had it and the program did not link. And
// `sizeof(Holder<3>::values)` is the member's size: `Holder` starts a
// type, but the name read on is no type.
#include <stdio.h>
#include <string.h>

static int f(int i) { return i * 2 + (int)strlen("ab"); }

template<int N> struct Holder {
    static const int values[N];
    static const int one;
    static int plain[N];
    static const char *names[];
};
template<int N> const int Holder<N>::values[N] = { f(N), f(N + 1) };
template<int N> const int Holder<N>::one = f(N * 10);
template<int N> int Holder<N>::plain[N] = { 1, 2 };
template<int N> const char *Holder<N>::names[] = { "zero", "one", "two" };

template<class T> struct Table;
template<class T> struct Table<T *> { static T entries[3]; };
template<class T> T Table<T *>::entries[3] = { T(1), T(2), T(3) };

int main(int argc, char **)
{
    Holder<2>::plain[0] += 10;
    printf("%d %d %d %d %s\n", Holder<2>::values[1], Holder<2>::one, Holder<2>::plain[0], Holder<3>::plain[1], Holder<3>::names[2]);
    printf("%d %d %d\n", Holder<3>::values[2], (int)(sizeof(Holder<3>::values) / sizeof(int)), Holder<4>::one);
    Table<long *>::entries[2] *= 7;
    printf("%ld %g\n", Table<long *>::entries[2], Table<double *>::entries[0]);
    return argc;
}
