// A partial specialization's pattern takes the argument's type whole,
// its top-level const too: `P<T *>` matches no `int *const` (it took
// one, the const stripped), `K<const T>` does, `Box<vector<T>>` no
// const vector.
#include <stdio.h>
#include <type_traits>
#include <vector>

template<class T> struct K { static constexpr int v = 0; };
template<class T> struct K<const T> { static constexpr int v = 1; };
template<class T> struct K<const T *> { static constexpr int v = 2; };
template<class T> struct K<T *> { static constexpr int v = 3; };
template<class T> struct P { static constexpr int v = 0; };
template<class T> struct P<T *> { static constexpr int v = 3; };
template<class T> struct IsVector : std::false_type { };
template<class T> struct IsVector<std::vector<T>> : std::true_type { };
template<class T> struct Box { };
template<class T> int boxed(Box<T *>) { return 1; }
template<class T> int boxed(Box<T>) { return 2; }

int main(int argc, char **)
{
    printf("%d %d %d %d %d\n", K<int>::v, K<const int>::v, K<const int *>::v, K<int *>::v, K<int *const>::v);
    printf("%d %d %d\n", P<int *const>::v, P<const int *>::v, (int)std::is_pointer_v<int *const>);
    printf("%d %d %d\n", (int)IsVector<std::vector<int>>::value, (int)IsVector<const std::vector<int>>::value, (int)IsVector<std::remove_cv_t<const std::vector<int>>>::value);
    printf("%d %d\n", boxed(Box<int *>()), boxed(Box<int *const>()));
    return argc;
}
