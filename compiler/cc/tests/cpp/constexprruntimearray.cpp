// A constexpr function reading a const array whose elements are made
// when the program starts is no constant: a static initialized by its
// call is made at run time too. Evaluated at compile time it read the
// array's bytes, zeros until the program started.
#include <array>
#include <stdio.h>
#include <string.h>

static int f(int i) { return i * 2 + (int)strlen("ab"); }

const int table[] = { f(1), f(2), 7 };
constexpr int get(int i) { return i < 0 ? 0 : table[i]; }

const std::array<int, 3> stdTable { f(3), f(4), 9 };
constexpr int getStd(int i) { return stdTable[i]; }

struct Pair { int a; int b; };
const Pair pairs[] = { { f(5), 2 }, { 3, f(6) } };
constexpr int getPair(int i) { return pairs[i].a + pairs[i].b; }

constexpr int constants[] = { 11, 12, 13 };
constexpr int getConstant(int i) { return constants[i]; }

int main(int argc, char **)
{
    static const int fromTable = get(1) * 10;
    static const int fromStd = getStd(0) + getStd(2);
    static const int fromPairs = getPair(0) + getPair(1);
    static const int last = get(2);
    static_assert(getConstant(1) == 12);
    static const int fromConstants = getConstant(2);
    int y = get(0);
    printf("%d %d %d %d %d %d\n", fromTable, fromStd, fromPairs, last, fromConstants, y);
    return argc;
}
