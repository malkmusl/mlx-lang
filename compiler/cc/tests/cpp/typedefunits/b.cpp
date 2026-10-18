// The functions a.cpp calls.
#include "units.h"

int apply(VTable *table, int x) { return table->get(x) + table->base; }
int whole(Number n) { return n.i; }
int inner(ns::Inner *in) { return in->value; }
