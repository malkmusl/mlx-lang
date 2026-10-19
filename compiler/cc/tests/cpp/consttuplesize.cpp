// std::tuple_size and std::tuple_element of a const tuple-like class are
// its class's (`std::tuple_size_v<std::remove_reference_t<T>>` with T a
// `const Position &`, WTF::apply of WebCore's style types); of a class
// without one, none (still no value in SFINAE).
#include <stdio.h>
#include <tuple>
#include <type_traits>
#include <utility>

struct Position { int x; int y; };
namespace std {
template<> struct tuple_size<Position> : integral_constant<size_t, 2> { };
template<size_t I> struct tuple_element<I, Position> { using type = int; };
}
template<size_t I> int get(const Position &p) { return I ? p.y : p.x; }

template<class F, class T, size_t... I> int applyImpl(F &&f, T &&t, std::index_sequence<I...>) { return f(get<I>(t)...); }
template<class F, class T> int apply(F &&f, T &&t) { return applyImpl(f, t, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>> { }); }

template<class T, class = void> constexpr bool hasSize = false;
template<class T> constexpr bool hasSize<T, std::void_t<decltype(std::tuple_size<T>::value)>> = true;

int main(int argc, char **argv)
{
    const Position p { 3, 4 };
    printf("%d\n", apply([](int a, int b) { return a * 10 + b; }, p));
    printf("%d %d %d %d\n", (int)std::tuple_size_v<const std::pair<int, long>>, (int)std::tuple_size_v<const std::tuple<int, int, int>>, (int)hasSize<const Position>, (int)hasSize<const int>);
    return argc;
}
