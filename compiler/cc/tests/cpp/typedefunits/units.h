// Unnamed classes a typedef names, in a header two files include after
// different numbers of other unnamed classes (JSC's GLib API declares
// `typedef struct { ... } JSCClassVTable;` in a C header): the classes
// take the typedefs' names for linkage, so a function taking one is the
// same function in both files.
#include <stdio.h>

typedef struct {
    int (*get)(int);
    int base;
} VTable;

typedef union {
    int i;
    float f;
} Number;

namespace ns {
typedef struct {
    int value;
} Inner;
}

int apply(VTable *table, int x);
int whole(Number n);
int inner(ns::Inner *in);
