// An rvalue argument for a template's parameter by value and for a
// function's `const T &` is converted alike ([over.ics.rank]): the
// function wins over the template's instance (JavaScriptCore's
// `makeString(const IndexOrName &)`, found by argument-dependent lookup,
// over WTF's `makeString(StringTypes...)` for
// `makeString(callee->indexOrName())`), and still `F(int)` over
// `template<class C> F(C &&)`, `T &&` over `const T &`.
#include <stdio.h>

namespace text {
template<typename T> int make(T) { return 1; }
template<typename... Ts> int makeAll(Ts...) { return 1; }
}
using text::make;
using text::makeAll;

namespace wasm {
struct Name { int index; };
int make(const Name&) { return 2; }
int makeAll(const Name&) { return 2; }
struct Callee {
    Name name;
    Name nameOf() const { return name; }
};
}

template<typename C> int forward(C&&) { return 1; }
int forward(int) { return 2; }

int bind(const wasm::Name&) { return 1; }
int bind(wasm::Name&&) { return 2; }

int main()
{
    wasm::Callee callee { { 7 } };
    wasm::Name lvalue { 3 };
    printf("%d %d %d %d %d %d\n", make(lvalue), make(callee.nameOf()), makeAll(callee.nameOf()), make(5), forward(5), bind(callee.nameOf()));
    return 0;
}
