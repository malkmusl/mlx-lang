// An explicit specialization of a class template naming itself in its
// members' bodies (`LayerSpace<IVector>({-fData.fX, -fData.fY})`, as
// Skia's SkImageFilterTypes.h): the class being read, not an instance of
// the primary template; the braces to the rvalue reference's constructor.
#include <cstdio>
#include <cstdint>
#include <utility>

struct SkIVector { int32_t fX, fY; };

struct IVector {
    int32_t fX;
    int32_t fY;
    IVector() = default;
    IVector(int32_t x, int32_t y) : fX(x), fY(y) {}
    explicit IVector(const SkIVector &v) : fX(v.fX), fY(v.fY) {}
};

template <typename T> class LayerSpace {};

template <> class LayerSpace<IVector> {
public:
    LayerSpace() = default;
    explicit LayerSpace(const IVector &geometry) : fData(geometry), fMoved(false) {}
    explicit LayerSpace(IVector &&geometry) : fData(std::move(geometry)), fMoved(true) {}
    LayerSpace<IVector> operator-() const { return LayerSpace<IVector>({-fData.fX, -fData.fY}); }
    int32_t x() const { return fData.fX; }
    int32_t y() const { return fData.fY; }
    bool moved() const { return fMoved; }
private:
    IVector fData;
    bool fMoved;
};

int main(int argc, char **argv) {
    IVector v(3, -4);
    LayerSpace<IVector> a(v);
    LayerSpace<IVector> b = -a;
    printf("%d %d %d %d\n", b.x(), b.y(), a.moved(), b.moved());
    return argc;
}
