// An inline or static function's body waits for its first use (mlx-cc
// reads it then). Used first from the body of a member function defined
// in its class, it is read in its own namespace: `values` in `get` is
// the global array, not the class's member function `values`.
#include <cstdio>

static int values[] = { 10, 20, 30 };
static int other = 7;
static inline int get(int index) { return values[index] + other; }

namespace inner {
static int values[] = { 1, 2, 3 };
inline int pick(int index) { return values[index]; }
}

struct Holder {
    int values() const { return 99; }
    int other() const { return 1; }
    int first() const { return get(1) + inner::pick(2); }
};

int main()
{
    std::printf("%d %d %d\n", Holder().first(), Holder().values(), Holder().other());
    return 0;
}
