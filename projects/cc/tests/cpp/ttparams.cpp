// Template template parameters: a template as an argument (Apply<Box,
// int>), deduced from an instance in a function template (sizeOf) and in
// partial specializations (Count<L<Ts...>>, Rewrap<From<Ts...>, To>),
// used with other arguments in the instance (To<Ts...>); using enum.
#include <stdio.h>
template <class T> struct Box { T v; Box(T x) : v(x) {} int size() const { return (int)sizeof(T); } };
template <class T> struct Pair { T a, b; Pair(T x) : a(x), b(x) {} int size() const { return 2 * (int)sizeof(T); } };
template <template <class> class C, class T> struct Apply { typedef C<T> type; };
template <template <class> class C, class T> int sizeOf(const C<T> &c) { return c.size(); }
template <class L> struct Count;
template <template <class...> class L, class... Ts> struct Count<L<Ts...> > { static const int value = sizeof...(Ts); };
template <class... Ts> struct List {};
template <class From, template <class...> class To> struct Rewrap;
template <template <class...> class From, class... Ts, template <class...> class To> struct Rewrap<From<Ts...>, To> { typedef To<Ts...> type; };
namespace color { enum class Kind { Red = 1, Green = 2, Blue = 4 }; }
int main() {
    Apply<Box, int>::type b(3);
    Apply<Pair, short>::type p(4);
    printf("%d %d %d %d\n", b.v, b.size(), p.a + p.b, sizeOf(p));
    printf("%d %d\n", Count<List<int, char, double> >::value, Count<List<> >::value);
    printf("%d\n", (int)sizeof(Rewrap<List<int, char>, List>::type));
    using enum color::Kind;
    printf("%d %d\n", (int)Green, (int)(Red) + (int)Blue);
    return 0;
}
