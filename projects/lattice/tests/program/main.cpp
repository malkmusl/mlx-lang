// The program test: two units translated as one Mlx program. A record, a
// template's instances (Box<int>, std::vector<Point>, std::map<int,
// Box<int>>) and an inline function of the shared header live in one of
// the units and are named from the other; a class is made in one unit
// and called and deleted through its base in the other; each unit has a
// helper of its own by the same name.
#include "shared.h"

namespace program {

namespace {
int helper(int v) { return v * 3; }
}

int run() {
    std::vector<Point> points;
    points.push_back(Point{1, 2});
    points.push_back(Point{10, 20});
    int result = total(points);

    std::map<int, Box<int>> table;
    Box<int> box;
    box.set(7);
    table[4] = box;
    box.set(twice(9));
    table[5] = box;
    result += lookup(table, 4) * 1000 + lookup(table, 5) * 10 + lookup(table, 6);

    Shape *first = makeSquare(3);
    Shape *second = makeSquare(4);
    result += first->area() * 100000 + second->area() * 7 + first->id + second->id;
    delete first;
    delete second;
    return result + helper(gMade);
}

}
