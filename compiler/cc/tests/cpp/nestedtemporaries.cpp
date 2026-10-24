// `T x(U(V(...)))`: an object initialized by temporaries, not a function
// declaration, when what the inner parentheses hold cannot be a
// declarator: `h.get()` (WebKit's FrameSelection), a call of a function
// on a value (RenderSVGText's `LayoutPoint(flooredIntPoint(p))`).
#include <cstdio>
struct Node { int v; };
struct Holder { Node* p; Node* get() const { return p; } Node* operator->() const { return p; } };
struct Position {
    enum Kind { A, B };
    Position(Node* n, unsigned o, Kind k) : n(n), o(o), k(k) {}
    Node* n; unsigned o; Kind k;
};
struct VisiblePosition {
    VisiblePosition(const Position& p, int aff = 0) : p(p), aff(aff) {}
    Position p; int aff;
};
struct Point { int x; };
struct LayoutPoint { LayoutPoint(Point p) : x(p.x * 2) {} int x; };
struct Location { Location(LayoutPoint p) : x(p.x + 1) {} int x; };
Point floored(double d) { return Point { (int)d }; }

int main() {
    Node node { 7 };
    Holder h { &node };
    Holder* hp = &h;
    unsigned index = 3;
    double where = 4.5;
    VisiblePosition before(VisiblePosition(Position(h.get(), index, Position::A)));
    VisiblePosition after(VisiblePosition(Position(h.get(), index + 1, Position::B), 1));
    VisiblePosition arrow(VisiblePosition(Position(hp->get(), 9, Position::B)));
    Location location(LayoutPoint(floored(where)));
    Location literal(LayoutPoint(floored(2.0)));
    std::printf("%d %u %d %u %d\n", before.p.n->v, before.p.o, after.p.k, after.p.o, after.aff);
    std::printf("%u %d %d\n", arrow.p.o, location.x, literal.x);
    return 0;
}
