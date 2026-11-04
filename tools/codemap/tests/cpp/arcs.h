// A header whose declarations one unit's macros leave out: the unit that
// keeps them writes its shard again (mlx-cc's S records).
#pragma once

#if SHAPES_ARCS
int arcLength(int degrees, int radius);
#endif
