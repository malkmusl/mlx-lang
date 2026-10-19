// A template parameter named as the concept constraining it
// (`template<typename IDL, DefaultValueFunctor<IDL> DefaultValueFunctor>`,
// WebCore's JSDOMConvertOptional.h): the constraint names the concept,
// not the parameter ("undeclared identifier").
#include <stdio.h>
#include <concepts>
#include <type_traits>

namespace W {
template<typename F, typename T> concept DefaultValueFunctor = std::is_invocable_r_v<T, F>;

template<typename T, DefaultValueFunctor<T> DefaultValueFunctor>
T withDefault(bool given, T value, DefaultValueFunctor&& defaultValue)
{
    if (!given)
        return defaultValue();
    return value;
}

template<typename T, DefaultValueFunctor<T> DefaultValueFunctor>
T withDefault2(bool given, T value, int extra, DefaultValueFunctor&& defaultValue)
{
    if (!given)
        return defaultValue() + extra;
    return value;
}
}

template<typename F> concept Small = sizeof(F) <= 8;
template<Small Small> int size(Small) { return (int)sizeof(Small); }
namespace A::B {
template<typename F> concept Callable = std::is_invocable_v<F>;
template<Callable Callable> int call(Callable c) { return c() * 2; }
}

int main(int argc, char **argv)
{
    printf("%d %d\n", W::withDefault<int>(false, 1, [] { return 7; }), W::withDefault2<int>(false, 1, 2, [] { return 7; }));
    printf("%d %d\n", size(5L), A::B::call([] { return 4; }));
    return argc;
}
