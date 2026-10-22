// `c ? object : value` where the value is converted to the object's class
// by a constructor (WebCore's Region::unite, `m_shape ? *m_shape :
// m_bounds`, a Shape made from an IntRect): the result is made from the
// value at its own place, not moved there by its bytes from a temporary,
// so an object pointing into itself (a Vector's inline buffer) still does.
#include <cstdio>
static int live = 0;
struct Rect { int x, w; };
struct Shape {
    int* data; int inlineValue;
    Shape(const Rect& r) : data(&inlineValue), inlineValue(r.w) { live++; std::printf("  Shape(Rect %d)\n", r.w); }
    Shape(const Shape& o) : data(&inlineValue), inlineValue(*o.data) { live++; std::printf("  Shape copy %d\n", inlineValue); }
    ~Shape() { live--; std::printf("  ~Shape %d %s\n", inlineValue, data == &inlineValue ? "ok" : "BAD"); }
    static int unite(const Shape& a, const Shape& b) { return *a.data + *b.data; }
};
int main()
{
    Shape s(Rect { 0, 5 });
    Shape* p = &s;
    Shape* q = nullptr;
    Rect r1 { 0, 7 };
    std::printf("shape\n"); std::printf("= %d\n", Shape::unite(p ? *p : r1, s));
    std::printf("rect\n"); std::printf("= %d\n", Shape::unite(q ? *q : r1, s));
    std::printf("live %d\n", live);
    return 0;
}
