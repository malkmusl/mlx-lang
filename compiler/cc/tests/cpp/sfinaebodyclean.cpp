// A function template's instance made while a candidate's signature has
// failed already (`t.missing()` named nothing, the rest of the decltype
// read on) reads its body from a clean start: the failure was the
// candidate's. Read with it, `std::is_integral_v<T>` in the body was the
// failed candidate's false, so the static_assert failed and the `if
// constexpr` took the other branch for every later call (WebCore's
// WTF::move<Calculation::Child &> in StylePrimitiveNumericTypes+Blending).
#include <stdio.h>
#include <type_traits>
#include <utility>

template<class T> auto twice(T t)
{
    static_assert(std::is_integral_v<T>, "integral");
    return t * 2;
}

template<class T> auto kind(T)
{
    if constexpr (std::is_integral_v<T>)
        return 10;
    else
        return 20;
}

struct Plain { int v; };

template<class T> auto probe(T t, int) -> decltype(t.missing(), twice(1)) { return 1; }
template<class T> long probe(T, long) { return 2; }

template<class T> auto probe2(T t, int) -> decltype(twice(t.missing()) + twice(2)) { return 1; }
template<class T> long probe2(T, long) { return 3; }

template<class T> auto probe3(T t, int) -> decltype(t.missing(), kind(1L)) { return 1; }
template<class T> long probe3(T, long) { return 4; }

int main(int argc, char **)
{
    long a = probe(Plain { 1 }, 0);
    long b = probe2(Plain { 1 }, 0);
    long c = probe3(Plain { 1 }, 0);
    printf("%ld %ld %ld %d %d %d\n", a, b, c, twice(4), kind(5L), kind(1.5));
    return argc;
}
