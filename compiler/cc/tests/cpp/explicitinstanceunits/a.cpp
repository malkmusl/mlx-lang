#include <cstdio>
#include "instances.h"

int main()
{
    S s;
    V1 v1;
    V2 v2;
    s.visit(v1);
    std::printf("%d\n", s.visit(v2));
    C<3> c;
    std::printf("%d %d %d %d\n", c.f(), c.g(), C<3>::s, C<3>::Inner().h());
    std::printf("%d %g\n", twice(21), twice(1.25));
    ns::Box<long> box { 5 };
    box.set(box.get() + 37);
    std::printf("%ld\n", box.get());
    return 0;
}
