// A function's name is an lvalue in C++: given to a forwarding reference
// `T&&`, T is a reference to the function's type (C's designator is no
// lvalue, and mlx-cc deduced the function type itself). WTF::Function
// is made of a function so: its constructor for callables takes only
// rvalues (`std::is_rvalue_reference_v<C&&>`), so a function takes the
// one for function pointers (PAL's TextCodecCJK returns its unencodable
// handlers as Functions).
#include <cstdio>
#include <type_traits>
#include <utility>

static void handler(int x) { printf("handler %d\n", x); }
static int twice(int x) { return 2 * x; }

template<typename T> const char* kind(T&&)
{
    if (std::is_lvalue_reference_v<T>)
        return std::is_function_v<std::remove_reference_t<T>> ? "function reference" : "lvalue reference";
    return std::is_function_v<T> ? "function" : "value";
}

template<typename T> int callForwarded(T&& f, int x) { return std::forward<T>(f)(x); }

template<typename F> struct Function;
template<typename Out, typename... In> struct Function<Out(In...)> {
    template<typename C>
        requires (!(std::is_pointer_v<C> && std::is_function_v<std::remove_pointer_t<C>>) && std::is_rvalue_reference_v<C&&>)
    Function(C&&) { which = "callable"; }

    template<typename P>
        requires (std::is_pointer_v<P> && std::is_function_v<std::remove_pointer_t<P>>)
    Function(P p) : pointer(p) { which = "function pointer"; }

    const char* which = "none";
    Out (*pointer)(In...) = nullptr;
};

static Function<void(int)> handlerFunction() { return handler; }

int main()
{
    int value = 1;
    printf("handler: %s\n", kind(handler));
    printf("value: %s\n", kind(value));
    printf("temporary: %s\n", kind(3));
    printf("pointer: %s\n", kind(&handler));
    printf("forwarded: %d\n", callForwarded(twice, 21));
    auto f = handlerFunction();
    printf("Function of a function: %s\n", f.which);
    f.pointer(7);
    Function<void(int)> g([](int) { });
    printf("Function of a lambda: %s\n", g.which);
    return 0;
}
