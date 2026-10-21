// A lambda's use of a local of the function around (array->put) where a
// using-declaration brings std::array in: the variable hides the template.
#include <array>
#include <stdio.h>
namespace W { using std::array; }
using namespace W;
struct J { int v = 0; void put(int x) { v += x; } };
template<typename F> void each(const F& f) { f(1); f(2); }
static array<int, 2> table = { 1, 2 };
int run() {
    J* array = new J;
    each([&] (int x) { array->put(x); });
    return array->v;
}
int main() { printf("%d %d\n", run(), table[1]); return 0; }
