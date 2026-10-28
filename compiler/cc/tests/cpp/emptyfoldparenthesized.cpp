// A binary fold over an empty pack whose pattern is parenthesized and
// names the pack's elements in a type: (0 + ... + (Count<Arg<Is>>::value))
// of JSC's CCallHelpers for an operation taking nothing.
#include <stdio.h>
#include <utility>
#include <tuple>
#include <type_traits>
template<typename F> struct Traits;
template<typename R, typename... A> struct Traits<R(*)(A...)> {
    static constexpr size_t arity = sizeof...(A);
    template<size_t I> using ArgumentType = std::tuple_element_t<I, std::tuple<A...>>;
};
template<typename T> using GPRCount = std::conditional_t<std::is_same_v<T, double>, std::integral_constant<unsigned, 0>, std::integral_constant<unsigned, 1>>;
struct H {
    template<typename TT, size_t... Is> static constexpr unsigned gprs(std::index_sequence<Is...>) { return (0 + ... + (GPRCount<typename TT::template ArgumentType<Is>>::value)); }
    template<typename Op> static unsigned count() { using T = Traits<Op>; return gprs<T>(std::make_index_sequence<T::arity>()); }
};
double f0() { return 1; }
double f3(int, double, long) { return 1; }
int main() { printf("%u %u\n", H::count<double(*)()>(), H::count<double(*)(int, double, long)>()); return 0; }
