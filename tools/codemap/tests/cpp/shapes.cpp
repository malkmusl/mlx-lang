#include "shapes.h"
#include "shapes_api.h"

namespace geo {

int Circle::area() const { return PI * SQUARE(radius); }

int count(int shapes) { return shapes + 1; }

int count(const Shape &shape) { return shape.id > 0 ? 1 : 0; }

namespace detail {
int scaled(int value) { return shapes_scale(value); }
}

}

static int total(geo::Shape &shape) {
    geo::Box<int> box;
    box.put(shape.area());
    return box.get() + geo::count(shape) + geo::detail::scaled(2);
}

int main() {
    geo::Circle circle(2);
    circle.id = 7;
    geo::Shape &shape = circle;
    return total(shape) + geo::count(3);
}
