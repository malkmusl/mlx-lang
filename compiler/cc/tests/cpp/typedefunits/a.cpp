// Calls the functions b.cpp defines; two unnamed classes come before the
// header here and none in b.cpp.
struct { int x; } first = { 1 };
union { int y; char c; } second = { 2 };

#include "units.h"

static int twice(int x) { return x * 2; }

int main() {
    VTable table = { twice, first.x };
    Number n;
    n.i = 7 + second.y;
    ns::Inner in = { 5 };
    printf("apply %d\n", apply(&table, 20));
    printf("whole %d\n", whole(n));
    printf("inner %d\n", inner(&in));
    return 0;
}
