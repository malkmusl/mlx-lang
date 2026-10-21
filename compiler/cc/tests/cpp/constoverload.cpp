// Member function templates overloaded by const alone (Skia's GrOp:
// `const T &cast() const` and `T *cast()`): a non-const object takes the
// non-const one.
#include <cstdio>

class Op {
public:
    virtual ~Op() {}
    template <typename T> const T &cast() const { return *static_cast<const T *>(this); }
    template <typename T> T *cast() { return static_cast<T *>(this); }
    int fId = 1;
};

class ClearOp : public Op {
public:
    int fBuffer = 7;
    bool combine(Op *t) {
        auto other = t->cast<ClearOp>();
        other->fBuffer += 1;
        return other->fBuffer == fBuffer;
    }
};

int main(int argc, char **argv) {
    ClearOp a, b;
    a.fBuffer = 8;
    const Op &constant = b;
    const ClearOp &viewed = constant.cast<ClearOp>();
    const bool combined = a.combine(&b);
    printf("%d %d %d\n", combined, b.fBuffer, viewed.fBuffer);
    return argc;
}
