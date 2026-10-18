// The detection idiom: partial specializations over void_t<decltype(...)>,
// whose non-deduced arguments are checked once the others are deduced
// (invoke_result, is_invocable, has_size), as mpark's variant does.
#include <stdio.h>
#include <type_traits>
#include <utility>
template <typename T> struct identity { using type = T; };
template <typename... Ts> struct voider : identity<void> {};
template <typename... Ts> using void_t = typename voider<Ts...>::type;
template <typename Void, typename, typename...> struct invoke_result {};
template <typename F, typename... Args>
struct invoke_result<void_t<decltype(std::declval<F>()(std::declval<Args>()...))>, F, Args...>
    : identity<decltype(std::declval<F>()(std::declval<Args>()...))> {};
template <typename F, typename... Args> using invoke_result_t = typename invoke_result<void, F, Args...>::type;
template <typename Void, typename, typename...> struct is_invocable : std::false_type {};
template <typename F, typename... Args> struct is_invocable<void_t<invoke_result_t<F, Args...>>, F, Args...> : std::true_type {};
struct Add { int operator()(int a, int b) const { return a + b; } };
struct Size { long operator()(const char *) const { return 1; } };
template <typename T, typename = void> struct has_size : std::false_type {};
template <typename T> struct has_size<T, void_t<decltype(std::declval<T>().size())>> : std::true_type {};
struct V { int size() const { return 3; } };
int main() {
    printf("%d %d %d\n", (int)is_invocable<void, Add, int, int>::value, (int)is_invocable<void, Add, const char *>::value, (int)is_invocable<void, Size, const char *>::value);
    printf("%d %d\n", (int)sizeof(invoke_result_t<Add, int, int>), (int)sizeof(invoke_result_t<Size, const char *>));
    printf("%d %d\n", (int)has_size<V>::value, (int)has_size<int>::value);
    return 0;
}
