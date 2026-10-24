// A block's `using namespace` seen in the member functions of a local
// class (Skia's SkMesh: `using namespace SkSL;` then a Visitor class
// testing `p.is<StructDefinition>()`).
#include <cstdio>

namespace sl {
struct Element {
    int kind;
    template <typename T> bool is() const { return kind == T::kKind; }
};
struct StructDefinition { static constexpr int kKind = 3; };
struct Visitor {
    virtual ~Visitor() {}
    virtual bool visitElement(const Element &) = 0;
};
}

static int countStructs(const sl::Element *elements, int n) {
    using namespace sl;
    class Counter final : public Visitor {
    public:
        bool visitElement(const Element &e) override {
            if (e.is<StructDefinition>()) ++fCount;
            return false;
        }
        int fCount = 0;
    } counter;
    for (int i = 0; i < n; i++) counter.visitElement(elements[i]);
    return counter.fCount;
}

int main(int argc, char **argv) {
    sl::Element elements[3] = {{3}, {1}, {3}};
    printf("%d\n", countStructs(elements, 3));
    return argc;
}
