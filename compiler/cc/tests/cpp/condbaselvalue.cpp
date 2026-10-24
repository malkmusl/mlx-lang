// A conditional of an lvalue of a class and an lvalue of a class derived
// from it is an lvalue of the base (Skia's SkFontHost_FreeType:
// `proxy ? *proxy : *this` passed through make_unique to a constructor
// taking SkTypeface &).
#include <cstdio>
#include <memory>
#include <type_traits>

struct Typeface {
    virtual ~Typeface() {}
    virtual int id() const { return 1; }
    int uses = 0;
};

struct FreeTypeface : Typeface {
    int id() const override { return 2; }
};

struct Padding {
    virtual ~Padding() {}
    long pad[3] = {1, 2, 3};
};

// (The base not at the start of the derived object.)
struct Multi : Padding, Typeface {
    int id() const override { return 3; }
};

struct Context {
    Context(const FreeTypeface &real, Typeface &proxy) : real(real), proxy(proxy) { proxy.uses += 1; }
    const FreeTypeface &real;
    Typeface &proxy;
};

static std::unique_ptr<Context> make(FreeTypeface &self, Typeface *proxy) {
    return std::make_unique<Context>(self, proxy ? *proxy : self);
}

int main(int argc, char **argv) {
    FreeTypeface self;
    Typeface other;
    auto a = make(self, nullptr);
    auto b = make(self, &other);
    Typeface *none = nullptr;
    static_assert(std::is_same_v<decltype(none ? *none : self), Typeface &>);
    (argc > 5 ? other : self).uses += 10;
    Multi multi;
    Typeface &picked = argc > 5 ? other : multi;
    picked.uses += 100;
    printf("%d %d %ld\n", picked.id(), multi.uses, multi.pad[2]);
    printf("%d %d %d %d\n", a->proxy.id(), b->proxy.id(), self.uses, other.uses);
    return argc;
}
