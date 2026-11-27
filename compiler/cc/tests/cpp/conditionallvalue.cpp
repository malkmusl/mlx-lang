// A conditional expression of two lvalues of one type is an lvalue: it
// is assigned to, compound-assigned to and incremented, and its address is
// taken (the conformance suite's `(cond ? a : b) = value`).
#include <stdio.h>

struct Pair
{
    int first;
    int second;
};

int main(int argc, char **)
{
    int a = 1, b = 2;
    (argc > 1 ? a : b) = 10;
    (argc > 5 ? a : b) = 20;
    Pair p = {3, 4};
    (argc > 1 ? p.first : p.second) += 5;
    ++(argc > 5 ? p.first : p.second);
    int *where = &(argc > 1 ? a : b);
    *where += 1;
    printf("%d %d %d %d\n", a, b, p.first, p.second);
    return 0;
}
