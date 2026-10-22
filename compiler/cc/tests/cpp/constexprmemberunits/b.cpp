#include <stdio.h>
#include "units.h"
const Headroom* fromImageInB() { return &Headroom::FromImage; }
int main()
{
    printf("%d %g %g %g\n", fromImageInA() == fromImageInB(), Headroom::FromImage.headroom, Headroom::None.headroom, epsilonInA() + W::epsilon.headroom);
    return 0;
}
