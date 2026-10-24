// A captureless lambda without a parameter list whose result is another
// lambda (`[] { return [] (auto &context) { ... }; }`, WebCore's
// SWClientConnection.cpp, given as a Function<Function<void(...)>()>):
// its body read again for the conversion to a pointer to a function
// makes another closure, the invoker's own result.
#include <stdio.h>
#include <utility>

template<typename> class Function;
template<typename R, typename... A> class Function<R(A...)> {
public:
    struct Base { virtual ~Base() { } virtual R call(A...) = 0; };
    template<typename F> struct Impl : Base {
        F f;
        Impl(F &&g) : f(std::move(g)) { }
        R call(A... a) override { return f(a...); }
    };
    template<typename F> Function(F f) : m(new Impl<F>(std::move(f))) { }
    Function(Function &&other) : m(other.m) { other.m = nullptr; }
    ~Function() { delete m; }
    R operator()(A... a) const { return m->call(a...); }
    Base *m;
};

struct Context {
    int id;
    void update() { printf("update %d\n", id); }
};

void forEach(const Function<Function<void(Context &)>()> &callback)
{
    Context c { 7 };
    callback()(c);
}

int main(int argc, char **)
{
    forEach([] { return [] (auto &context) { context.update(); }; });
    auto make = [] { return [] (Context &context) { context.id += 1; return context.id; }; };
    Context c { 3 };
    printf("%d\n", make()(c));
    auto counted = [] { int k = 5; return [k] (Context &context) { return context.id * k; }; };
    printf("%d\n", counted()(c));
    auto pointer = +make;
    printf("%d\n", pointer()(c));
    return argc;
}
