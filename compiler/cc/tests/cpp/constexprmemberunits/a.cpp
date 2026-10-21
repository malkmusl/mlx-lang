#include "units.h"
const Headroom* fromImageInA() { return &Headroom::FromImage; }
float epsilonInA() { return W::epsilon.headroom; }
