// Enum bit-fields: zero-extended when the enum has no fixed type and no negative enumerator, as GCC reads them; sign-extended for enum class and enum E : int
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

namespace p_ebf2 {
enum class C { A, B };
enum F : int { FA, FB };
enum class U : unsigned { UA, UB };
struct S { C c : 1; F f : 1; U u : 1; };
int run() {
    S s{};
    s.c = C::B; s.f = FB; s.u = U::UB;
    printf("%d %d %d %d\n", (int)s.c, (int)s.f, (int)s.u, s.c == C::B);
    return 0;
}
}

int main() {
    p_ebf43::run();
    p_ebf2::run();
    return 0;
}
