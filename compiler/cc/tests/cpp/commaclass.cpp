// A comma expression whose value is a class's prvalue, `(check(), f())`
// (WTF::visit returns `(valueless ? throw : (void)0), visit_value(...)`):
// returned, initializing an object, bound to a reference, assigned. f's
// result is the one object: neither destroyed under the value nor
// copied by its bytes.
#include <cstdio>
#include <utility>
#include <cstdlib>

static int live = 0;

struct Str {
    int* refs;
    Str(int v) : refs(new int(1)) { (void)v; live++; }
    Str(const Str& o) : refs(o.refs) { ++*refs; }
    Str(Str&& o) : refs(std::exchange(o.refs, nullptr)) { }
    Str& operator=(Str&& o) { Str m(std::move(o)); std::swap(refs, m.refs); return *this; }
    ~Str() { if (refs && --*refs == 0) { live--; delete refs; } }
    int count() const { return refs ? *refs : -1; }
};

static void check(bool ok) { if (!ok) abort(); }
static Str make() { return Str(1); }

static Str viaComma(bool ok) {
    return (ok ? (void)0 : check(false)), make();
}

static decltype(auto) viaCommaAuto(bool ok) {
    return (ok ? (void)0 : check(false)), make();
}

static Str viaCommaLocal() {
    Str s(2);
    return (void)0, s;
}

static int byRvalue(Str&& s) { Str kept(std::move(s)); return kept.count(); }
static int byConst(const Str& s) { return s.count(); }
static int byValue(Str s) { return s.count(); }

int main(int argc, char** argv) {
    printf("rvalue %d const %d value %d live %d\n", byRvalue(((void)0, make())), byConst(((void)0, make())), byValue(((void)0, make())), live);
    {
        Str e(((void)0, make()));
        printf("direct init refs %d live %d\n", e.count(), live);
        Str f = Str(2);
        f = ((void)0, make());
        printf("assigned refs %d live %d\n", f.count(), live);
    }
    {
        Str a = viaComma(true);
        printf("comma refs %d live %d\n", a.count(), live);
        Str b = viaCommaAuto(true);
        printf("comma auto refs %d live %d\n", b.count(), live);
        Str c = viaCommaLocal();
        printf("comma local refs %d live %d\n", c.count(), live);
        Str d = ((void)0, make());
        printf("comma init refs %d live %d\n", d.count(), live);
    }
    printf("live %d args %d\n", live, argc);
    return live;
}
