#include <cstdio>
#include <tuple>
#include <type_traits>

namespace lib {
template<typename... T> struct list { };
struct _1 { };
namespace detail {
template<int N, typename S, typename... F> struct transform;
template<template<typename...> class Seq, typename... T, template<typename...> class Func>
struct transform<0, Seq<T...>, Func<_1>> { using type = Seq<typename Func<T>::type...>; };
}
template<typename S, typename F, typename... More>
using transform = typename detail::transform<sizeof...(More), S, F, More...>::type;
}

namespace app {
template<typename T> struct Mapping { using type = T*; };
template<typename D, unsigned I> struct Getter { using List = lib::list<D, char>; };
template<typename L> struct Single;
template<typename T, typename U> struct Single<lib::list<T, U>> { using type = std::tuple<T, U>; };
template<typename L> using SingleOf = typename Single<L>::type;

template<typename D, unsigned I>
using Mapped = lib::transform<typename Getter<D, I>::List, Mapping<lib::_1>>;
template<typename D, unsigned I>
using Result = SingleOf<Mapped<D, I>>;
template<typename D>
using Parsed = std::tuple<Result<D, 0>, Result<D, 1>>;

template<typename D> constexpr int count(const Parsed<D>&) { return 2; }

template<typename A, typename... More> using Count = std::integral_constant<int, sizeof...(More)>;
template<typename D> constexpr int counted(Count<D, int, char> c) { return c.value + 10; }
template<typename D> constexpr int countedNone(Count<D> c) { return c.value + 20; }
}

int main()
{
    app::Parsed<int> p { };
    std::printf("%d %d\n", (int)std::is_same_v<app::Result<int, 0>, std::tuple<int*, char*>>, app::count<int>(p));
    std::printf("%d %d\n", app::counted<long>({ }), app::countedNone<long>({ }));
    return 0;
}
