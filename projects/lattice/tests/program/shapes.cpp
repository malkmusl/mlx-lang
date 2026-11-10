// The program test's second unit: what shared.h declares, and a helper of
// its own with the name of one in main.cpp.
#include "shared.h"

namespace program {

int gMade = 0;

namespace {
int helper(int v) { return v + 100; }

class Square : public Shape {
  public:
    explicit Square(int side) : mSide(side) {}
    int area() const override { return mSide * mSide; }

  private:
    int mSide;
};
}

Shape *makeSquare(int side) {
    gMade++;
    Shape *shape = new Square(side);
    shape->id = helper(gMade);
    return shape;
}

int total(const std::vector<Point> &points) {
    int sum = 0;
    for (const Point &p : points) {
        sum += twice(p.sum());
    }
    return sum;
}

int lookup(const std::map<int, Box<int>> &table, int key) {
    auto found = table.find(key);
    return found == table.end() ? -1 : found->second.get();
}

}
