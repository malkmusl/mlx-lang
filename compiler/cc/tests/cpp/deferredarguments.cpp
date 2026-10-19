// A class template's instance named as a template argument is named
// only, made when something needs it whole ([temp.inst]): WTF's
// EnumValueChecker ends its recursion on `EnumValues<E>`, whose
// `minmax({ values... })` of no values is no constant (WebCore's
// platform-13 stopped there). What needs it whole makes it: alignof
// (SegmentedVector's `AlignedStorage<T, alignof(T)>` of a pair named as
// an argument was aligned to 1), sizeof, members, objects.
#include <algorithm>
#include <stddef.h>
#include <stdio.h>
#include <type_traits>
#include <utility>

template<typename E, E... values>
struct EnumValues {
private:
    static constexpr auto boundsImpl = std::minmax({ values... });
public:
    static constexpr E min = boundsImpl.first;
    static constexpr E max = boundsImpl.second;
    static constexpr size_t count = sizeof...(values);
};
template<typename T, typename E> struct EnumValueChecker;
template<typename T, typename E, E e, E... es>
struct EnumValueChecker<T, EnumValues<E, e, es...>> {
    static constexpr bool isValid(T t)
    {
        return (static_cast<T>(e) == t) || EnumValueChecker<T, EnumValues<E, es...>>::isValid(t);
    }
};
template<typename T, typename E>
struct EnumValueChecker<T, EnumValues<E>> {
    static constexpr bool isValid(T) { return false; }
};
enum class Color : unsigned char { Red, Green = 4, Blue = 9 };
using Values = EnumValues<Color, Color::Red, Color::Green, Color::Blue>;

template<class T> struct Holder { T *p = nullptr; };
template<class T> struct Never { static_assert(sizeof(T) > 100, "never made"); };
template<class T> struct UsesSize { char c[sizeof(T)]; };

template<class T, size_t A> struct Aligned { alignas(A) unsigned char bytes[sizeof(T)]; };
template<class T> struct Segment { Aligned<T, alignof(T)> storage[2]; };
template<class A, class B> struct Pair { A a; B b; };

template<class T> struct Counted { static int made; int v = 3; };
template<class T> int Counted<T>::made = 7;

int main(int argc, char **)
{
    printf("%d %d %zu\n", (int)Values::min, (int)Values::max, Values::count);
    printf("%d %d %d\n", EnumValueChecker<unsigned char, Values>::isValid(4), EnumValueChecker<unsigned char, Values>::isValid(5), EnumValueChecker<unsigned char, Values>::isValid(9));
    Holder<Never<int>> h;
    printf("%d %d\n", h.p == nullptr, (int)std::is_same_v<Holder<Never<char>>, Holder<Never<char>>>);
    UsesSize<Holder<Never<long>>> u;
    printf("%zu\n", sizeof u.c);
    Segment<Pair<const char *, int>> s;
    printf("%zu %zu\n", alignof(decltype(s.storage[0])), sizeof s);
    printf("%zu %zu\n", alignof(Holder<Pair<double, char>>), alignof(Pair<char, Holder<int>>));
    Holder<Counted<int>> c;
    Counted<int> counted;
    printf("%d %d %d\n", c.p == nullptr, Counted<int>::made, counted.v);
    return argc;
}
