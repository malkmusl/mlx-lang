#include <stdio.h>
#include "units.h"
const Headroom* fromImageInB() { return &Headroom::FromImage; }
int main()
{
    printf("%d %g %g\n", fromImageInA() == fromImageInB(), Headroom::FromImage.headroom, Headroom::None.headroom);
    return 0;
}
