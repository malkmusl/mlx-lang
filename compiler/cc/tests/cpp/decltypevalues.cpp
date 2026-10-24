// decltype of a class's value made by `T()` is T (it was read as the
// function type `T()`), of a comma expression its right operand's; a
// called member a class lacks fails the candidate in SFINAE, its
// arguments read (Brigand's has_at_method, `decltype(C::at(*p),
// std::true_type())`: "expected ')'" in every JSBindings unit).
#include <stdio.h>
#include <type_traits>

struct T { };
int f();
T g();
T& h();

namespace br {
template<class... X> struct list { };
struct withAt { template<class P> static int at(P) { return 1; } };
namespace detail {
template <typename X>
struct has_at_method {
    struct dummy {};
    template <typename C, typename P>
    static auto test(P * p) -> decltype(C::at(*p), std::true_type());
    template <typename, typename>
    static std::false_type test(...);
    static constexpr bool value = std::is_same<std::true_type, decltype(test<X, dummy>(nullptr))>::value;
};
template <class L, typename Index, bool> struct at_dispatch { using type = int; };
template <class L, typename Index> struct at_dispatch<L, Index, true> { using type = long; };
}
template <class Seq, typename K> using at = typename detail::at_dispatch<Seq, K, detail::has_at_method<Seq>::value>::type;
}

int main(int argc, char **argv)
{
    printf("%d %d %d %d\n", (int)std::is_same_v<T, decltype(f(), T())>, (int)std::is_same_v<T, decltype(T())>, (int)std::is_same_v<T, decltype(f(), g())>, (int)std::is_same_v<T&, decltype(f(), h())>);
    printf("%d %d\n", (int)std::is_same_v<int, decltype(1, 2)>, (int)std::is_same_v<T&&, decltype(f(), T())>);
    printf("%d %d\n", (int)sizeof(br::at<br::list<int, char>, int>), (int)br::detail::has_at_method<br::withAt>::value);
    return argc;
}
