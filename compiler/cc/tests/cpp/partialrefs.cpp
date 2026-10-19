// A partial specialization naming one parameter twice (`is_same<T, T>`)
// matches only the same type twice: a reference and the type it refers
// to were taken for the same (`std::is_same_v<T &&, T>` held), as a
// function template's explicit `f<int &>(x)` for `T &` is.
#include <stdio.h>
#include <type_traits>

struct T { };
template<class A, class B> struct Same { static constexpr int v = 0; };
template<class A> struct Same<A, A> { static constexpr int v = 1; };

int main(int argc, char **argv)
{
    printf("%d %d %d %d\n", (int)std::is_same_v<int&&, int>, (int)std::is_same_v<int, int&&>, (int)std::is_same_v<T&&, T>, (int)std::is_same_v<T, T&&>);
    printf("%d %d %d %d\n", Same<int&&, int>::v, Same<T&&, T>::v, Same<T&, T>::v, Same<T&&, T&>::v);
    printf("%d %d\n", (int)std::is_same_v<T&, T&>, (int)std::is_same_v<const T&, T&>);
    return argc;
}
