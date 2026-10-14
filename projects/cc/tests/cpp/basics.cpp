// C++ without classes: namespaces and qualified names, using,
// overloading with the usual conversions, default arguments, references
// (as parameters, results and locals), bool, scoped enums, extern "C",
// __cplusplus. g++ -std=c++17 printed the expected output.
#include <stdio.h>
#include <string.h>

namespace geometry {
    int scale = 3;
    int area(int w, int h) { return w * h * scale; }
    double area(double w, double h) { return w * h; }
    namespace inner {
        const char *name() { return "inner"; }
        int depth() { return 2; }
    }
    int useInner() { return inner::depth() + scale; }
}

namespace alias = geometry::inner;

int twice(int x) { return 2 * x; }
long twice(long x) { return 3 * x; }
double twice(double x) { return 2.5 * x; }
const char *twice(const char *s) { return s; }

int sum(int a, int b = 10, int c = 100) { return a + b + c; }

void bump(int &x, int by) { x += by; }
int &pick(int &a, int &b, bool first) { return first ? a : b; }
int square(const int &x) { return x * x; }

enum class Color { Red, Green = 5, Blue };
enum Plain { One = 1, Two };

extern "C" int cfunc(int x) { return x + 1000; }

static int counter = 0;
int next() { return ++counter; }

int main(int argc, char **argv)
{
    printf("%d %d\n", geometry::area(2, 3), (int)geometry::area(2.0, 3.0));
    printf("%s %d %d\n", geometry::inner::name(), alias::depth(), geometry::useInner());
    using namespace geometry;
    printf("%d %d\n", area(4, 5), scale);
    using geometry::inner::name;
    printf("%s\n", name());
    printf("%d %ld %.1f %s\n", twice(7), twice(7L), twice(2.0), twice("str"));
    printf("%d %d %d\n", sum(1), sum(1, 2), sum(1, 2, 3));
    int v = 5;
    bump(v, 7);
    int w = 1;
    int &chosen = pick(v, w, false);
    chosen = 42;
    printf("%d %d %d\n", v, w, square(v));
    const int &cref = v + 1;
    printf("%d\n", cref);
    bool flag = v > 3;
    bool other = false;
    printf("%d %d %zu %d\n", flag, other, sizeof(bool), (int)(flag + flag));
    Color c = Color::Blue;
    printf("%d %d %d %d\n", (int)c, (int)Color::Green, One, Two);
    printf("%d %d\n", cfunc(1), next() + next());
    printf("%ld %d %d\n", __cplusplus, argc, (int)strlen(argv[0]) > 0);
    printf("%zu %zu\n", sizeof(int &), sizeof(char *));
    return 0;
}
