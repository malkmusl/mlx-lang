// The g++ side: makes its results and checks ours.
#include "shared.h"

Self::Self(int v) : data(&inlineValue), inlineValue(v) { }
Self::Self(const Self& o) : data(&inlineValue), inlineValue(*o.data) { }
Self::~Self() { }

Self gppSelf(int v) { return Self(v * 3); }
Pair gppPair(long a) { return { a, a * 2 }; }

int gppChecks()
{
    Self s = mlxSelf(5);
    Pair p = mlxPair(6);
    return (s.home() ? 1 : 0) + (s.value() == 10 ? 2 : 0) + (p.a == 6 && p.b == 7 ? 4 : 0);
}
