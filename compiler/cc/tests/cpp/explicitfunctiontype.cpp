// A parameter `Function<T(const char *, int)>` of a function template
// whose T is given explicitly is known: the argument (a lambda) converts
// to it, nothing is deduced from it (JSC's InspectorDebuggerAgent calls
// `parseBreakpointOptions<std::optional<ProtocolBreakpoint>>(..., [&]
// (...) -> std::optional<ProtocolBreakpoint> { ... })`).
#include <optional>
#include <stdio.h>
#include <utility>

template<class F> class Function;
template<class R, class... A> class Function<R(A...)> {
public:
    template<class L> Function(L l) : m_call(new Holder<L>(std::move(l))) { }
    ~Function() { delete m_call; }
    R operator()(A... a) const { return m_call->call(a...); }
private:
    struct Base { virtual ~Base() { } virtual R call(A...) = 0; };
    template<class L> struct Holder : Base { L l; Holder(L&& x) : l(std::move(x)) { } R call(A... a) override { return l(a...); } };
    Base *m_call;
};

template<class T> static T parse(const char *text, Function<T(const char *, int)> callback) { return callback(text, 2); }
template<class T, class U> static T convert(U u, Function<T(U)> f) { return f(u); }

int main(int argc, char **)
{
    int scale = 10;
    auto a = parse<std::optional<int>>("abc", [&](const char *s, int n) -> std::optional<int> { return s[0] * scale + n; });
    auto b = parse<long>("x", [](const char *s, int n) { return (long)s[0] - n; });
    auto c = convert<double, int>(7, [](int x) { return x / 2.0; });
    printf("%d %ld %.1f\n", *a, b, c);
    return argc;
}
