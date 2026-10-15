// Class member aliases (`using T = ...;` in a class, by the class's name
// and through a class template's instance), a name of an inline
// namespace qualified by its parent from inside it, generic callables
// (a member operator() template), an explicit reference template
// argument, enable_if aliases differing only in their condition, an
// overload whose body would not compile left unread, and a constructor
// template's brace initializer.
#include <stdio.h>
#include <type_traits>
#include <utility>

struct S { using Ty = int; Ty x; };
template <typename T> struct identity { using type = T; };
template <typename T> int fv(typename identity<T>::type t) { return t + 1; }

namespace lib {
inline namespace v1 {
template <typename T> T &&forward(std::remove_reference_t<T> &t) noexcept { return static_cast<T &&>(t); }
template <typename T> T &&forward(std::remove_reference_t<T> &&t) noexcept {
    static_assert(!std::is_lvalue_reference<T>::value, "can not forward an rvalue as an lvalue");
    return static_cast<T &&>(t);
}
struct equal_to {
    template <typename L, typename R>
    constexpr auto operator()(L &&l, R &&r) const -> decltype(lib::forward<L>(l) == lib::forward<R>(r)) {
        return lib::forward<L>(l) == lib::forward<R>(r);
    }
};
template <typename F, typename... As> auto invoke(F &&f, As &&... as) -> decltype(lib::forward<F>(f)(lib::forward<As>(as)...)) {
    return lib::forward<F>(f)(lib::forward<As>(as)...);
}
}
}

template <typename T> T &&pass(T &t) { return static_cast<T &&>(t); }

template <class T> std::enable_if_t<std::is_integral_v<T>, int> kind(T) { return 1; }
template <class T> std::enable_if_t<!std::is_integral_v<T>, int> kind(T) { return 2; }

struct all_same {
    const bool value = false;
    constexpr all_same(...) {}
    template <typename T> constexpr all_same(T *) : value{ true } {}
};

int main() {
    S s; s.x = 4;
    S::Ty y = 5;
    identity<long>::type z = 6;
    printf("aliases %d %d %d %d\n", s.x, y, (int)z, fv<int>(1));
    int a = 1, b = 1, c = 2;
    lib::equal_to eq;
    printf("generic %d %d %d\n", (int)eq(a, b), (int)eq(a, c), (int)lib::invoke(eq, b, c));
    int &r = pass<int &>(a);
    r = 9;
    printf("reference %d %d\n", a, lib::forward<int &>(a));
    printf("kinds %d %d %d\n", kind(3), kind(2.5), kind('c'));
    int n = 0;
    all_same one(&n), other(1.5);
    printf("brace %d %d\n", (int)one.value, (int)other.value);
    return 0;
}
