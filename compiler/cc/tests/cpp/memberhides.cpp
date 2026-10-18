// A base's member function called by the name of a type around the
// class (Skia's IRHelpers::Field beside struct SkSL::Field), in a local
// class's member function: the member function.
#include <cstdio>
#include <memory>

namespace sl {
struct Field {
    Field(int pos, int layout) : fPos(pos + layout) {}
    int fPos;
};

struct Expression { int value; };

struct Helpers {
    std::unique_ptr<Expression> Field(int owner, int index) const { return std::make_unique<Expression>(Expression{owner * 10 + index}); }
};

static int fixup(int owner) {
    struct Fixup : public Helpers {
        int fOwner;
        std::unique_ptr<Expression> Pos() const { return Field(fOwner, 2); }
    };
    Fixup f;
    f.fOwner = owner;
    sl::Field plain(1, 2);
    return f.Pos()->value + plain.fPos;
}
}

int main(int argc, char **argv) {
    printf("%d\n", sl::fixup(4));
    return argc;
}
