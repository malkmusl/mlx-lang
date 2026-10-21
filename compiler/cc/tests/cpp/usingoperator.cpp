// `using N::operator<<;` in a block (and in a namespace): the operator's
// functions usable there, found for `out << x` where argument-dependent
// lookup would not find them.
#include <stdio.h>

struct Out { int sum = 0; };
namespace N {
Out &operator<<(Out &o, int v) { o.sum += v; return o; }
Out &operator<<(Out &o, const char *s) { o.sum += 100 * (s[0] - '0'); return o; }
bool operator==(const Out &a, const Out &b) { return a.sum == b.sum; }
}

namespace M {
using N::operator==;
bool same(const Out &a, const Out &b) { return a == b; }
}

int main(int argc, char **)
{
    using N::operator<<;
    Out o;
    o << 3 << 4 << "2";
    Out p;
    p << 207;
    printf("%d %d %d\n", o.sum, p.sum, M::same(o, p));
    return argc;
}
