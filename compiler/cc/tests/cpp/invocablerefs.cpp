// A call of one function in SFINAE checks how its reference parameters
// bind: `f(int &&)` takes no `int &`, `g(int &)` no rvalue, so
// std::is_invocable of them is false (WTF::switchOn's visitors taking
// `T &&` meet this).
#include <stdio.h>
#include <type_traits>
#include <utility>

void takesRvalue(int &&) { }
void takesLvalue(int &) { }
void takesConst(const int &) { }
struct Name { int length; };
struct Visitor { int operator()(Name &&name) const { return name.length; } };
struct LvalueVisitor { int operator()(Name &name) const { return name.length; } };

template<class F, class A, class = decltype(std::declval<F>()(std::declval<A>()))> int call(int) { return 1; }
template<class F, class A> int call(long) { return 0; }

int main(int argc, char **argv)
{
    printf("%d %d %d\n", (int)std::is_invocable_v<decltype(takesRvalue), int &>, (int)std::is_invocable_v<decltype(takesRvalue), int>, (int)std::is_invocable_v<decltype(takesRvalue), int &&>);
    printf("%d %d\n", (int)std::is_invocable_v<decltype(takesLvalue), int>, (int)std::is_invocable_v<decltype(takesLvalue), int &>);
    printf("%d %d\n", (int)std::is_invocable_v<decltype(takesConst), int>, (int)std::is_invocable_v<decltype(takesConst), int &>);
    printf("%d %d %d\n", call<Visitor, Name &>(0), call<Visitor, Name>(0), call<LvalueVisitor, Name>(0));
    printf("%d\n", call<LvalueVisitor, Name &>(0) + argc);
    Name name { argc + 3 };
    return Visitor { }(std::move(name)) - 2 - argc;
}
