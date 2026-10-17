// Enum bit-fields read back zero-extended when no enumerator is negative (as GCC has it)
#include <stdio.h>

namespace p_ebf43 {
enum E { A, B };
typedef enum E E;
enum N { M = -1, P = 0 };
struct S { E c : 1; E s : 1; unsigned f : 2; unsigned idx : 28; enum N n : 1; };
int run() {
    struct S s = {};
    s.s = B; s.c = B; s.f = 0; s.n = M;
    struct S *p = &s;
    printf("%d %d %d %d %d\n", p->s == B, p->c == B, (int)p->s, (int)p->n, p->s);
    return 0;
}
}

int main() {
    p_ebf43::run();
    return 0;
}
