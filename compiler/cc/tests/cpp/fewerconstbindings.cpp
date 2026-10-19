// Of two function templates taking a modifiable lvalue as well, the one
// binding it to a reference without const wins before the more
// specialized one ([over.ics.rank]): WTF's `map(S &&)` over `map(const
// FixedVector<T> &, F &&)` for a FixedVector variable (TouchList.h's
// "no operator= for these operands"). For a const object or a
// temporary, the more specialized still.
#include <stdio.h>
#include <type_traits>
#include <utility>

template<typename T> struct Fixed { T v[2]; };

template<typename F, typename S> int map(S &&source, F &&f) { return 1; }
template<typename T, typename F, typename R = typename std::invoke_result<F, const T &>::type> int map(const Fixed<T> &source, F &&f) { return 3; }

template<typename T> int size(T &) { return 1; }
template<typename T> int size(const Fixed<T> &) { return 2; }

template<typename T> int pick(T &, const T &) { return 1; }
template<typename T> int pick(const Fixed<T> &, const Fixed<T> &) { return 2; }

int main(int argc, char **argv)
{
    Fixed<int> touches { { 1, 2 } };
    const Fixed<int> constant { { 1, 2 } };
    auto f = [](auto &t) -> long { return t; };
    printf("%d %d %d\n", map(touches, f), map(constant, f), map(Fixed<int> {}, f));
    printf("%d %d\n", size(touches), size(constant));
    printf("%d %d\n", pick(touches, constant), pick(constant, touches));
    return argc;
}
