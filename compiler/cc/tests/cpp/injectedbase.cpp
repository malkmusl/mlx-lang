// A base class's name inside a derived class in another namespace: the
// base's injected class name, found as a member (Skia's
// SkSVGOpenTypeSVGDecoder: `using INHERITED = ResourceProvider;` of a
// skresources::ResourceProvider).
#include <cstdio>

namespace resources {
class Provider {
public:
    virtual ~Provider() {}
    virtual int load() const { return 1; }
};
template <typename T>
class Holder {
public:
    T value{};
};
}

namespace {
class DataProvider final : public resources::Provider {
public:
    int load() const override { return INHERITED::load() + 10; }
    Provider *self() { return this; }
private:
    using INHERITED = Provider;
};

class IntHolder : public resources::Holder<int> {
public:
    int get() const { return Holder::value + 1; }
    using Base = Holder<int>;
};
}

int main(int argc, char **argv) {
    DataProvider p;
    IntHolder h;
    h.value = 4;
    IntHolder::Base &b = h;
    printf("%d %d %d %d\n", p.load(), p.self()->load(), h.get(), b.value);
    return argc;
}
