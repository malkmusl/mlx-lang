// A class returned by value from a function whose result `decltype(auto)`
// (or `const auto`) deduces: made in the caller's buffer, not copied
// there by its bytes. A vector with inline capacity points into itself;
// copied by its bytes it pointed into the callee's temporary (WebCore's
// toStyleOnTupleLike through std::apply: a gradient's color stops).
#include <stdio.h>
#include <string.h>
#include <tuple>
#include <utility>
#include <functional>

struct Vec {
    int* buf;
    int size;
    int inl[2];
    Vec() : buf(inl), size(0) { }
    Vec(Vec&& o) : buf(inl), size(o.size) { memcpy(inl, o.inl, sizeof inl); o.size = 0; }
    Vec(const Vec& o) : buf(inl), size(o.size) { memcpy(inl, o.inl, sizeof inl); }
    void append(int v) { buf[size++] = v; }
    bool ok() const { return buf == inl; }
};

struct Agg {
    int a;
    Vec v;
};

static int conv(int x) { return x + 1; }
static Vec conv(const Vec& v) { Vec r; r.append(v.buf[0] * 10); return r; }

template<class F> decltype(auto) call(F&& f) { return f(); }
template<class F> decltype(auto) callForwarded(F&& f) { return std::invoke(std::forward<F>(f)); }
template<class F> const auto callConst(F&& f) { return f(); }

template<typename To, typename From> To toTuple(const From& t)
{
    return std::apply([&](const auto&... x) { return To { conv(x)... }; }, t);
}

int main()
{
    auto make = [] { Vec r; r.append(7); return r; };
    Vec a = call(make);
    Vec b = callForwarded(make);
    const Vec c = callConst(make);
    printf("%d %d %d %d %d %d\n", a.ok(), a.buf[0], b.ok(), b.buf[0], c.ok(), c.buf[0]);
    Vec three;
    three.append(3);
    std::tuple<int, Vec> t { 1, three };
    Agg g = toTuple<Agg>(t);
    printf("%d %d %d\n", g.v.ok(), g.a, g.v.buf[0]);
    return 0;
}
