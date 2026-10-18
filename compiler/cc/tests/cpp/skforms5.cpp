// Forms of Skia's: a braced list as a compound assignment's operand (`p +=
// {dx, dy}`), an unnamed local class deriving from a base (`class :
// public GlobalStructVisitor { ... } visitor;`).
#include <cstdio>
#include <vector>

struct Vec {
    float x, y;
    Vec &operator+=(const Vec &v) { x += v.x; y += v.y; return *this; }
    Vec &operator=(const Vec &) = default;
};

struct Visitor {
    virtual ~Visitor() {}
    virtual int visit(int value) { return value; }
};

static int walk(Visitor *v) { return v->visit(4) + v->visit(5); }

int main(int argc, char **argv) {
    std::vector<Vec> pts = {{1, 2}, {3, 4}};
    float dx = 0.5f, dy = 1;
    for (auto &p : pts) { p += {dx, dy}; }
    Vec offset{0, 0};
    offset += {0, 7};
    offset = {offset.x + 1, offset.y};
    class : public Visitor {
    public:
        int visit(int value) override { return value * fScale; }
        int fScale = 3;
    } visitor;
    printf("%.1f %.1f %.0f %.0f %d\n", pts[1].x, pts[0].y, offset.x, offset.y, walk(&visitor));
    return argc;
}
