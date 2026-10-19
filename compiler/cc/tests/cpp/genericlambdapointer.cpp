// A captureless generic lambda converts to a pointer to a function: its
// call operator's instance for the pointer's parameters, as a static
// invoker (WebCore's bindings pass `[] (auto& server) ->
// JSC::HeapCellType& { ... }` as a `HeapCellType& (*)(JSHeapData&)`). The
// conversion was missing: an argument found no candidate, and a pointer
// initialized from the closure was the closure's bytes (a crash).
#include <stdio.h>

struct Data {
    int cell;
    int other;
};

template<class T, class Get> int sub(Get get, int &(*custom)(Data &) = nullptr)
{
    Data d { 4, 5 };
    return get(d) + (custom ? custom(d) * 10 : 0);
}

static int apply(int (*f)(int), int x) { return f(x); }
static double applyDouble(double (*f)(double), double x) { return f(x); }

int main(int argc, char **)
{
    int a = sub<int>([](auto &d) { return d.other; }, [](auto &server) -> int & { return server.cell; });
    int b = sub<int>([](auto &d) { return d.other; });
    auto triple = [](auto x) { return x * 3; };
    int (*g)(int) = triple;
    long (*h)(long) = [](auto x) { return x + 100; };
    printf("%d %d %d %ld %d %g\n", a, b, g(4), h(5), apply(triple, 6), applyDouble(triple, 1.5));
    return argc;
}
