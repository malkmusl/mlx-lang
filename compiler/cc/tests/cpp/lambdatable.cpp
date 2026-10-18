// Captureless lambdas in a table of pointers to functions (a structure's
// members, as GLib's GSourceFuncs are filled), and one for a pointer alone.
#include <cstdio>

typedef int (*Fn)(int);
struct Funcs { Fn first; Fn second; void (*third)(int*); };

struct Holder { static Funcs s_funcs; };

Funcs Holder::s_funcs = {
    [](int x) -> int { return x + 1; },
    nullptr,
    [](int* p) { *p = 7; },
};

static Fn single = [](int x) { return x * 2; };

int main()
{
    int v = 0;
    Holder::s_funcs.third(&v);
    std::printf("%d %d %d %d\n", Holder::s_funcs.first(1), Holder::s_funcs.second == nullptr, v, single(4));
    return 0;
}
