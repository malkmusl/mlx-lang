// The implicit copy of a class with a reference member and members with
// constructors (member by member): the copy's reference refers to what
// the source's does, as WTF's string adapters (pad()) need it.
#include <cstdio>
#include <algorithm>
struct Spec { unsigned char character; unsigned length; int value; };
struct Inner { Inner(int v) : v(v) {} Inner(const Inner& o) : v(o.v) {} int v; };
struct Adapter {
    Adapter(const Spec& s) : m(s), u(s.value) {}
    const Spec& m;
    Inner u;
};
struct Moved { Moved(int& r) : r(r), u(1) {} int& r; Inner u; };
Adapter pass(Adapter a) { return a; }
int main()
{
    Spec spec { '0', 4, 1970 };
    Adapter a(spec);
    Adapter b = a;
    Adapter c = pass(b);
    printf("%u %u %d %d\n", b.m.length, c.m.length, c.m.value, &c.m == &spec);
    int x = 5;
    Moved m(x);
    Moved n = static_cast<Moved&&>(m);
    n.r = 7;
    printf("%d %d\n", x, n.u.v);
    return 0;
}
