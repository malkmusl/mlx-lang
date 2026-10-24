// The temporaries of an unevaluated operand (decltype, sizeof, noexcept)
// are never made, so never destroyed: `typedef decltype(f(1)) U;` in a
// body destroyed a string never made with the statement after it
// (std::optional::transform's: "munmap_chunk(): invalid pointer").
#include <stdio.h>

struct Loud {
    int v;
    Loud(int x) : v(x) { printf("make %d\n", v); }
    Loud(const Loud &o) : v(o.v) { printf("copy %d\n", v); }
    ~Loud() { printf("destroy %d\n", v); }
};
Loud make(int x) { return Loud(x); }

int typedefed()
{
    typedef decltype(make(1)) U;
    U u = make(2);
    return u.v;
}
int aliased()
{
    using U = decltype(make(3));
    return U(4).v;
}
int declared()
{
    decltype(make(5)) u = make(6);
    return u.v;
}
int sized()
{
    int n = sizeof(make(7));
    bool unused = noexcept(make(8));
    (void)unused;
    Loud u = make(9);
    return n + u.v;
}
template<typename F> int generic(F &&f)
{
    typedef decltype(f(10)) U;
    U u = f(11);
    return u.v;
}

int main(int argc, char **argv)
{
    printf("%d\n", typedefed());
    printf("%d\n", aliased());
    printf("%d\n", declared());
    printf("%d\n", sized());
    printf("%d\n", generic([](int x) { return make(x); }));
    return argc;
}
