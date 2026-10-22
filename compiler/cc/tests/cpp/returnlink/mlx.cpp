// Our side: makes its results and checks g++'s.
#include <cstdio>
#include "shared.h"

int gppChecks();

Self mlxSelf(int v) { return Self(v * 2); }
Pair mlxPair(long a) { return { a, a + 1 }; }

int main()
{
    Self s = gppSelf(4);
    Pair p = gppPair(9);
    std::printf("g++'s: %d %s, %ld %ld\n", s.value(), s.home() ? "home" : "left behind", p.a, p.b);
    std::printf("ours, as g++ sees them: %d\n", gppChecks());
    return 0;
}
