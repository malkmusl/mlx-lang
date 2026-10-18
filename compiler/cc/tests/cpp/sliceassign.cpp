// A derived object assigned to its base (the base's implicit copy
// assignment, slicing): `fCTM = op.matrix` of a TypedMatrix, `fDst =
// *fPixels` of an SkAutoPixmapStorage (Skia).
#include <cstdio>

class Matrix {
public:
    Matrix() : fScale(1), fTrans(0) {}
    Matrix(float s, float t) : fScale(s), fTrans(t) {}
    float apply(float x) const { return x * fScale + fTrans; }
private:
    float fScale, fTrans;
};
struct TypedMatrix : public Matrix {
    TypedMatrix() {}
    TypedMatrix(const Matrix &m) : Matrix(m) {}
};

class Pixmap {
public:
    Pixmap() : fAddr(nullptr), fWidth(0) {}
    int width() const { return fWidth; }
protected:
    void reset(void *addr, int w) { fAddr = addr; fWidth = w; }
    void *fAddr;
    int fWidth;
};
class AutoPixmapStorage : public Pixmap {
public:
    AutoPixmapStorage() {}
    AutoPixmapStorage &operator=(AutoPixmapStorage &&other) {
        reset(other.fAddr, other.fWidth);
        other.reset(nullptr, 0);
        return *this;
    }
    void alloc(int w) { reset(fBytes, w); }
private:
    char fBytes[16];
};

struct Draw { Pixmap fDst; };

int main(int argc, char **argv) {
    Matrix ctm;
    TypedMatrix typed(Matrix(2, 3));
    ctm = typed;
    AutoPixmapStorage storage;
    storage.alloc(4);
    AutoPixmapStorage *pixels = &storage;
    Draw draw;
    draw.fDst = *pixels;
    printf("%.0f %d\n", ctm.apply(5), draw.fDst.width());
    return argc;
}
