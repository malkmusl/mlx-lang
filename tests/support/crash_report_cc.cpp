// tools/check_crash_report.sh: a C++ program mlx-cc compiles that crashes
// on purpose three calls deep, in a member function of a class template
// in a namespace; its report must name each function, mangled and as it
// is written in C++, and where each was defined.
#include <stdio.h>

namespace geometry {
struct Shape { int *corner; int sides; };
static int depth = 0;
template <typename T> struct Holder {
    T value;
    int poke(Shape *shape, int by) { return *shape->corner + by + value; }
};
int measure(Shape *shape) {
    Holder<int> holder = { 3 };
    depth++;
    return holder.poke(shape, depth) * 2;
}
}

int outer(int count) {
    geometry::Shape shape = { nullptr, count };
    return geometry::measure(&shape) + 1;
}

int main(int argc, char **argv) {
    (void)argv;
    printf("about to crash\n");
    fflush(stdout);
    return outer(argc);
}
