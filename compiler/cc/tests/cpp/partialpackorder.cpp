// A partial specialization over `V<T>` is more specialized than one over
// `V<T, Types...>` for a single argument, V an alias of a variadic class
// template as WTF::Variant is of mpark::variant (alternative_index_helper:
// the pack one would make the empty variant<> its member names; through
// the alias `Variant<T>` is `variant<T...>`, T no pack).
#include <stdio.h>
#include <type_traits>
namespace mp { template<class... Ts> struct variant { static_assert(0 < sizeof...(Ts), "empty"); }; }
template<class... Ts> using V = mp::variant<Ts...>;
template<int N> struct C { };
template<class T> struct U { };
template<unsigned long, class, class> struct H;
template<unsigned long index, class Type, class T>
struct H<index, Type, V<T>> {
    static constexpr unsigned long count = std::is_same_v<Type, T>;
    static constexpr unsigned long value = index;
};
template<unsigned long index, class Type, class T, class... Types>
struct H<index, Type, V<T, Types...>> {
    static constexpr unsigned long count = std::is_same_v<Type, T> + H<index + 1, Type, V<Types...>>::count;
    static constexpr unsigned long value = std::is_same_v<Type, T> ? index : H<index + 1, Type, V<Types...>>::value;
};
template<class T, class Var> struct AI;
template<class T, class... Types> struct AI<T, V<Types...>> : std::integral_constant<unsigned long, H<0, T, V<Types...>>::value> {
    static_assert(H<0, T, std::remove_cv_t<V<Types...>>>::count == 1);
};
using Var = V<C<939>, C<941>, C<638>, U<int>>;
int main(int argc, char **argv) { printf("%lu %lu\n", AI<C<941>, Var>::value, AI<U<int>, Var>::value); return argc - 2; }
