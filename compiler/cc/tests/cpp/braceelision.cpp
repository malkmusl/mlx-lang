// Brace elision into member aggregates ([dcl.init.aggr]): an element
// that does not convert to the member aggregate initializes the
// aggregate's first member, and the elements after it the next ones
// (WebCore's `CSS::PathFunction { CSS::Keyword::Nonzero { },
// CSS::Path::Data { ... } }`, FunctionNotation's one member a Path).
#include <cstdio>
#include <optional>
#include <variant>
#include <vector>
struct Nonzero { };
struct Evenodd { };
using FillRule = std::variant<Nonzero, Evenodd>;
struct Path {
    struct Data { std::vector<int> bytes; };
    std::optional<FillRule> fillRule;
    Data data;
};
template<int C, typename T> struct Function {
    T parameters;
    const T* operator->() const { return &parameters; }
};
using PathFunction = Function<3, Path>;

struct Inner { int a; int b; };
struct Middle { Inner inner; int c; };
struct Outer { Middle middle; int d; };
struct Named { int v; };
struct Convert { operator Named() const { return Named { 9 }; } };
struct Holder { Named named; int e; };

int main() {
    std::vector<int> b { 1, 2, 3 };
    auto f = PathFunction { Nonzero { }, Path::Data { b } };
    PathFunction g { Evenodd { }, Path::Data { { 4 } } };
    PathFunction full { { Evenodd { }, Path::Data { { 5, 6 } } } };
    PathFunction h { Evenodd { } };
    std::printf("%d %zu %zu %zu %zu\n", f->fillRule.has_value(), f->fillRule->index(), g->fillRule->index(), f->data.bytes.size() + g->data.bytes.size(), h->data.bytes.size() + full->data.bytes.size());
    Outer o { 1, 2, 3, 4 };
    Outer p { { 5, 6 }, 7 };
    Outer q { 8 };
    std::printf("%d %d %d %d | %d %d %d %d | %d %d %d %d\n", o.middle.inner.a, o.middle.inner.b, o.middle.c, o.d,
        p.middle.inner.a, p.middle.inner.b, p.middle.c, p.d, q.middle.inner.a, q.middle.inner.b, q.middle.c, q.d);
    Holder k { Convert { }, 5 };
    Holder l { Named { 6 }, 7 };
    std::printf("%d %d %d %d\n", k.named.v, k.e, l.named.v, l.e);
    return 0;
}
