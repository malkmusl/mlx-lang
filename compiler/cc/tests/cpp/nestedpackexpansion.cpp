// A pack expansion whose pattern holds another pack expanded inside it:
// `F<typename apply<Ts, L, Ls...>::type...>` is over Ts, however many
// Ls there are (none: Brigand's apply of a lambda over one list, in
// WebCore's IDL union conversions).
#include <stdio.h>
#include <type_traits>

template<class... T> struct list { };
struct _1 { };
template<class T, class... Ls> struct apply { using type = T; };
template<class A, class... Args, class... Ls> struct apply<_1, list<A, Args...>, Ls...> { using type = A; };
template<template<class...> class F, class... Ts, class L, class... Ls>
struct apply<F<Ts...>, L, Ls...> : F<typename apply<Ts, L, Ls...>::type...> { };

template<class... Ts> struct count { static constexpr int value = sizeof...(Ts); };
template<class... Ts> struct sizes { static constexpr int value = (0 + ... + sizeof(Ts)); };

struct Object { };

int main(int argc, char **argv)
{
    printf("%d %d\n", (int)apply<std::is_same<char, _1>, list<char>>::value, (int)apply<std::is_same<Object, _1>, list<char>>::value);
    printf("%d %d\n", apply<count<_1, int, _1>, list<int>>::value, apply<count<>, list<int>, list<long>>::value);
    printf("%d\n", apply<sizes<char, short>, list<int>, list<long>>::value + argc);
    return argc - 2;
}
