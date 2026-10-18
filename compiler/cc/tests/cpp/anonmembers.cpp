// A member of an anonymous union named in its class's scope
// (`decltype(fBitfields)` in a static_assert, as Skia's SkPaint.h), and
// a base's operator called by its qualified name
// (`this->ResourceKey::operator=(that)`, as Skia's ResourceKey.h).
#include <cstdio>
#include <cstdint>
#include <type_traits>

class Paint {
public:
    Paint() { fBitfieldsUInt = 0; }
    void setStyle(unsigned s) { fBitfields.fStyle = s; }
    unsigned bits() const { return fBitfieldsUInt; }
private:
    union {
        struct {
            unsigned fAntiAlias : 1;
            unsigned fStyle : 2;
            unsigned fPadding : 29;
        } fBitfields;
        uint32_t fBitfieldsUInt;
    };
    static_assert(std::is_trivially_copyable<decltype(fBitfields)>::value);
    static_assert(sizeof(decltype(fBitfieldsUInt)) == 4);
};

class Key {
public:
    Key &operator=(const Key &that) {
        fValue = that.fValue + 100;
        return *this;
    }
    int fValue = 0;
};

class ScratchKey : public Key {
public:
    ScratchKey() {}
    ScratchKey(const ScratchKey &that) { *this = that; }
    ScratchKey &operator=(const ScratchKey &that) {
        this->Key::operator=(that);
        fExtra = that.fExtra + 1;
        return *this;
    }
    int fExtra = 0;
};

int main(int argc, char **argv) {
    Paint p;
    p.setStyle(3);
    ScratchKey a;
    a.fValue = 5;
    a.fExtra = 7;
    ScratchKey b(a);
    printf("%u %d %d\n", p.bits() >> 1, b.fValue, b.fExtra);
    return argc;
}
