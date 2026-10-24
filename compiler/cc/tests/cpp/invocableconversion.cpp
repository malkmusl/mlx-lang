// A call of one function in SFINAE fails when an argument of a class
// converts to no parameter: `std::is_invocable<int (*)(int), X>` held
// for a class X with no conversion to int (and a visitor's overload
// taking it was thought callable).
#include <stdio.h>
#include <functional>
#include <type_traits>

struct X { };
struct Y { operator int() const { return 1; } };
struct Visitor { int operator()(int) const { return 1; } };
int takesInt(int) { return 0; }
int takesPointer(const char *) { return 0; }

template<class F, class A> auto tryCall(F f, A a) -> decltype(f(a), 1) { return 1; }
template<class F> int tryCall(F, ...) { return 0; }

int main(int argc, char **)
{
    printf("%d %d %d\n", (int)std::is_invocable_v<int (*)(int), X>, (int)std::is_invocable_v<int (*)(int), Y>, (int)std::is_invocable_v<int (*)(int), long>);
    printf("%d %d %d\n", (int)std::is_invocable_v<Visitor, X>, (int)std::is_invocable_v<Visitor, char>, (int)std::is_invocable_v<decltype(takesInt), X>);
    printf("%d\n", (int)std::is_invocable_v<decltype(takesPointer), char *>);
    printf("%d %d\n", tryCall(takesInt, X {}), tryCall(takesInt, 3.5));
    std::function<int(int)> f = Visitor {};
    printf("%d %d\n", f(2), (int)std::is_constructible_v<std::function<int(X)>, Visitor>);
    return argc;
}
