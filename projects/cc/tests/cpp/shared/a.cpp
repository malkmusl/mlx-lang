#include "shared.h"
void bump() { single().n++; plain()++; tsingle<Counter>().n += 10; sharedCount += 3; twice()++; }
