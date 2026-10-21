// A name looked up and missed under a prefix (`f` seen from inside N,
// where N has no f yet, finds the global one) is remembered as missing:
// declaring it there afterwards, as a namespace member, a class member,
// a using-declaration or a using-directive brings in, must be seen by
// the next lookup, and the earlier readings keep what they found.
#include <stdio.h>

int f() { return 1; }
int g(int) { return 10; }
namespace N {
int callF1() { return f(); }
int f() { return 2; }
int callF2() { return f(); }
namespace Inner {
int callF3() { return f(); }
int callG1() { return g(0); }
int g(int) { return 20; }
int callG2() { return g(0); }
}
}

namespace M {
int h() { return 100; }
namespace Deep { int k() { return 1000; } }
}
int useH1() { return 0; }
using namespace M;
int useH2() { return h(); }
using namespace M::Deep;
int useK() { return k(); }

struct Base { int v() const { return 3; } };
int v() { return 4; }
struct Derived : Base {
    int before() const { return v(); }
    int v() const { return 5; }
    int after() const { return v(); }
};

// A name that is a type under one prefix and a value under another.
namespace T { struct S { int x; }; }
namespace U {
int S = 7;
int getS() { return S; }
}
namespace V {
using T::S;
int sizeS() { return sizeof(S); }
}

// A qualified name made by a path other than a declaration under its
// prefix: a template's instance named `P::Q<int>` is interned whole
// before `P::Q` is ever looked up as a pair.
namespace P {
template<typename T> struct Q { static int n() { return sizeof(T); } };
}
int qn() { return P::Q<int>::n() + P::Q<char>::n(); }

// Argument-dependent lookup finding a function declared after a first
// call of the same name with other arguments.
namespace A {
struct X { };
int pick(X) { return 30; }
}
struct Y { };
int pick(Y) { return 40; }
int callPick1() { return pick(Y { }); }
int callPick2() { return pick(A::X { }); }

// What a name meant is forgotten when the scope around changes: a
// using-directive in a block is dropped with the block; a lambda or a
// local class sees the function's types through the suspended scopes;
// a namespace entered again after names were declared into it.
namespace Q { int w(int) { return 50; } }
int w(long) { return 60; }
int blockUsing(int n)
{
    int a = w(n);
    int b = 0;
    int c = w(n + 1);
    if (n > 0) {
        using namespace Q;
        b = w(n);
    }
    return a * 10000 + b * 100 + c;
}
namespace R { struct Local { int x; }; }
int lambdaSees(int n)
{
    struct Local { int y; };
    auto f = [](int m) { Local l { m }; return l.y + sizeof(Local); };
    auto g = [](int m) { R::Local l { m }; return l.x; };
    return f(n) + g(n);
}
namespace N {
int later() { return f() + callF1(); }
}
int laterUse() { return N::later(); }

int main(int argc, char **argv)
{
    printf("%d %d %d\n", blockUsing(argc), (int)lambdaSees(argc), laterUse());
    printf("%d %d %d\n", N::callF1(), N::callF2(), N::Inner::callF3());
    printf("%d %d\n", N::Inner::callG1(), N::Inner::callG2());
    printf("%d %d %d\n", useH1(), useH2(), useK());
    Derived d;
    printf("%d %d %d\n", d.before(), d.after(), d.Base::v());
    printf("%d %d %d\n", U::getS(), (int)V::sizeS(), qn());
    printf("%d %d\n", callPick1(), callPick2());
    return argc;
}
