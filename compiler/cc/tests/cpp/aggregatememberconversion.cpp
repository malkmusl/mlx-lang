// Overloads given a braced list: an aggregate whose first member (a
// class with constructors) the first element does not convert to is no
// candidate (makeSimpleRange({ visibleStart, visibleEnd }) takes the
// VisiblePositionRange, not the PositionRange).
#include <cstdio>
#include <optional>
struct Node { int id; };
struct Position {
    Position() : node(nullptr), offset(0) { }
    Position(Node* n, int o) : node(n), offset(o) { }
    Node* node;
    int offset;
};
struct VisiblePosition {
    VisiblePosition(const Position& p) : position(p) { }
    Position position;
};
struct PositionRange { Position start; Position end; };
struct VisiblePositionRange { VisiblePosition start; VisiblePosition end; };
struct Range { int a, b; };
int makeSimpleRange(const PositionRange& r) { return 1 + r.end.offset; }
int makeSimpleRange(const VisiblePositionRange& r) { return 100 + r.start.position.offset + r.end.position.offset; }
int makeSimpleRange(const Range* r) { return r ? 3 : 4; }
int main() {
    Node n { 1 };
    VisiblePosition a(Position(&n, 2)), b(Position(&n, 5));
    printf("%d\n", makeSimpleRange({ a, b }));
}
