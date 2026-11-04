#include "arcs.h"

#if SHAPES_ARCS
int arcLength(int degrees, int radius)
{
    return degrees * radius * 314 / 18000;
}
#endif

int arcsUsed()
{
#if SHAPES_ARCS
    return arcLength(90, 2);
#else
    return 0;
#endif
}
