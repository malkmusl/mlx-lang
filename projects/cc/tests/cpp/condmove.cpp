// `c ? object : temporary` of a class with a destructor is a prvalue:
// the object is copied or moved into the result by its constructor
// (`c ? std::move(a) : T()` leaves `a` moved from), the temporary moved
// in; two unique_ptr types meet in the one the other converts to (JSC's
// IsoSubspace: `allocator ? WTF::move(allocator) : makeUnique<...>()`).
#include <memory>
#include <cstdio>
struct Base { virtual ~Base() { printf("~Base %d\n", id); } virtual int f() { return 1; } int id = 0; };
struct Fast : Base { Fast() { id = 7; } int f() override { return 2; } };
struct Holder {
    std::unique_ptr<Base> m;
    Holder(std::unique_ptr<Base>&& a) : m(a ? std::move(a) : std::make_unique<Fast>()) { }
};
struct Counted { int n; Counted(int v) : n(v) { } Counted(const Counted& o) : n(o.n + 100) { } Counted(Counted&& o) : n(o.n + 10) { o.n = -1; } ~Counted() { } };
int main(int argc, char**) {
    { auto a = std::make_unique<Base>(); a->id = 1; std::unique_ptr<Base> z = argc ? std::move(a) : std::unique_ptr<Base>(); printf("a=%d z=%d\n", (bool)a, z->id); }
    { auto a = std::make_unique<Base>(); a->id = 2; std::unique_ptr<Base> z = argc ? std::unique_ptr<Base>() : std::move(a); printf("a=%d z=%d\n", (bool)a, (bool)z); }
    { Holder h(nullptr); printf("f=%d\n", h.m->f()); }
    { auto b = std::make_unique<Base>(); b->id = 3; Holder h(std::move(b)); printf("f=%d b=%d\n", h.m->f(), (bool)b); }
    { Counted c(5); Counted d = argc ? c : Counted(6); Counted e = argc ? std::move(c) : Counted(6); printf("copied %d moved %d from %d\n", d.n / 100, e.n < 100 && e.n > 10, c.n); }
    printf("end\n");
    return 0;
}
