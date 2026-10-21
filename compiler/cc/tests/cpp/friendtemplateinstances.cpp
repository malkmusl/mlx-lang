// A friend function template defined in a class template: each instance
// of the class makes one from the same tokens, and each reads its names in
// its own instance (the injected `Base` there is that instance). WebCore's
// PrimitiveNumericWrapperBase defines `template<size_t> friend get` that
// way, and a later instance's scope made the earlier ones' `get` take the
// later instance, so `get<0>(x)` found no match for the earlier.
#include <stdio.h>
#include <stddef.h>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Style {
template<class T> struct Base;
template<class T> struct Base<T *> {
    T value;
    template<size_t> friend const auto &get(const Base &self) { return self.value; }
    friend T twice(const Base &self) { return self.value * 2; }
};
struct PositionX : Base<int *> { };
struct PositionY : Base<long *> { };
struct Scale : Base<double *> { };
}

namespace std {
template<> class tuple_size<Style::PositionY> : public std::integral_constant<size_t, 1> { };
template<size_t I> class tuple_element<I, Style::PositionY> {
public:
    using type = decltype(get<I>(std::declval<Style::PositionY>()));
};
}

template<size_t I, class T> auto pick(const T &t) { return get<I>(t); }

int main(int argc, char **)
{
    Style::PositionX x { { 3 } };
    Style::PositionY y { { 40000000000L } };
    Style::Scale s { { 1.5 } };
    static_assert(std::is_same_v<std::tuple_element_t<0, Style::PositionY>, const long &>);
    static_assert(std::is_same_v<decltype(get<0>(x)), const int &>);
    printf("%d %ld %g\n", get<0>(x), get<0>(y), get<0>(s));
    printf("%d %ld %g\n", pick<0>(x), pick<0>(y), pick<0>(s));
    printf("%d %ld %g\n", twice(x), twice(y), twice(s));
    auto [only] = y;
    printf("%ld %d\n", only, (int)sizeof(decltype(get<0>(s))));
    return argc;
}
