// Partial specializations chosen by the most specific pattern (pointers,
// const, the same parameter twice, a class template's instance, a
// value), explicit specializations declared before they are defined,
// alias templates, class templates declared only (an alias or a
// pointer to an instance before the definition), template parameters
// without names or typed by an earlier one, inline namespaces, a
// function template declared only and defined as another template.
#include <cstdio>

template <typename T> struct Kind { static const char *name() { return "other"; } };
template <typename T> struct Kind<T *> { static const char *name() { return "pointer"; } };
template <typename T> struct Kind<const T *> { static const char *name() { return "const pointer"; } };
template <typename T> struct Kind<T &> { static const char *name() { return "reference"; } };
template <> struct Kind<int> { static const char *name() { return "int"; } };
template <> struct Kind<bool>;
template <> struct Kind<bool> { static const char *name() { return "bool"; } };

template <typename A, typename B> struct Same { enum { value = 0 }; };
template <typename A> struct Same<A, A> { enum { value = 1 }; };

template <typename T, int N> struct Box { T items[N]; int size() const { return N; } };
template <typename T> struct Box<T, 0> { int size() const { return -1; } };
template <typename T, int N> struct Describe { static int depth() { return 0; } };
template <typename T, int N> struct Describe<Box<T, N>, N> { static int depth() { return N; } };
template <typename T> struct Describe<Box<T, 2>, 2> { static int depth() { return 22; } };

template <typename T, typename U = int> struct Pair { T a; U b; };
template <typename T> using IntPair = Pair<int, T>;
template <typename T> using Boxed = Box<T, 2>;

template <typename T> class Promise;
template <typename T> using Later = Promise<T>;
struct Holder { Later<int> *later; Promise<char> *promise; };
template <typename T> class Promise { public: T value; Promise() : value(7) {} };

template <typename, unsigned long = 0> class Deque;
template <typename Key, typename, Key last> struct Enumerated { Key k; Enumerated() : k(last) {} };

namespace std { namespace experimental { inline namespace fundamentals_v3 {
template <class T, class E> class expected { public: T v; E e; };
int level() { return 3; }
} } }
template <class T, class E> using Expected = std::experimental::expected<T, E>;

template <typename T> T twice(T x);
template <typename T> T twice(T x) { return x + x; }

int main() {
    int n = 1;
    int &r = n;
    (void)r;
    printf("%s %s %s %s %s %s %s\n", Kind<double>::name(), Kind<char *>::name(), Kind<const char *>::name(),
           Kind<int>::name(), Kind<int *>::name(), Kind<bool>::name(), Kind<int &>::name());
    printf("%d %d %d\n", Same<int, int>::value, Same<int, long>::value, Same<Box<int, 2>, Box<int, 2> >::value);
    Box<int, 3> b;
    Box<int, 0> z;
    printf("%d %d %d %d %d\n", b.size(), z.size(), Describe<Box<int, 3>, 3>::depth(), Describe<Box<int, 2>, 2>::depth(), Describe<int, 3>::depth());
    IntPair<double> p;
    p.a = 1;
    p.b = 2.5;
    Boxed<char> bc;
    bc.items[1] = 'x';
    printf("%d %g %c %d\n", p.a, p.b, bc.items[1], bc.size());
    Promise<int> a;
    Later<int> l;
    Holder h;
    h.later = &l;
    printf("%d %d\n", a.value, h.later->value);
    Enumerated<int, char, 9> e;
    Expected<int, char> x;
    x.v = 5;
    x.e = 'e';
    printf("%d %d %c %d %d\n", e.k, x.v, x.e, std::experimental::level(), std::experimental::fundamentals_v3::level());
    printf("%d %g\n", twice(21), twice(1.25));
    return 0;
}
