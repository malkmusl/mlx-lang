// A class returned by value from a function whose result type its
// returns deduce (`auto f()`, a lambda's): made right in the caller's
// buffer, as with a declared result type. The buffer was added after the
// body was read, so the returned object was made in a temporary and its
// bytes copied: a Vector with inline capacity kept pointing into the
// temporary (WebCore's media queries freed that pointer).
#include <cstdio>
#include <initializer_list>
struct Small {
    Small() : data(inlineData), size(0) { }
    Small(std::initializer_list<int> list) : data(inlineData), size(0) { for (int v : list) inlineData[size++] = v; }
    Small(const Small& o) : data(inlineData), size(o.size) { for (int i = 0; i < size; i++) inlineData[i] = o.data[i]; }
    Small(Small&& o) : data(inlineData), size(o.size) { for (int i = 0; i < size; i++) inlineData[i] = o.data[i]; o.size = 0; }
    ~Small() { if (data != inlineData) std::printf("bad buffer %s\n", tag); }
    int* data;
    int size;
    int inlineData[1];
    const char* tag = "";
};
auto autoFn(int c) { return Small { c }; }
Small direct(int c) { return Small { c }; }
Small twoReturns(int c) { if (c) return Small { 7 }; return Small { 2 }; }
Small forwarded(int c) { return twoReturns(c); }
struct V { virtual ~V() { } virtual Small call(int c) { return twoReturns(c); } };
int main()
{
    { auto a = direct(1); a.tag = "direct"; }
    { auto b = twoReturns(1); b.tag = "two1"; }
    { auto b = twoReturns(0); b.tag = "two0"; }
    { auto c = forwarded(1); c.tag = "forwarded"; }
    V v; V* p = &v;
    { auto d = p->call(1); d.tag = "virtual"; }
    auto l = [](int c) { if (c) return Small { 7 }; return Small { 2 }; };
    { auto e = l(1); e.tag = "lambda1"; }
    { auto e = l(0); e.tag = "lambda0"; }
    { auto f = autoFn(1); f.tag = "auto fn"; }
    auto one = [](int c) { return Small { c }; };
    { auto g = one(3); g.tag = "lambda one"; std::printf("%d %d\n", g.size, g.data[0]); }
    std::printf("done\n");
    return 0;
}
