// Pointers to members: data and member function pointers, .* and ->*,
// deduction of R T::* in templates, typedefs, a member of that type,
// null ones and the safe-bool conversion idiom.
#include <stdio.h>
#include <utility>
struct P { int x; int y; double z; int sum(int k) const { return x + y + k; } int twice() { return 2 * x; } };
template <typename R, typename T, typename Arg> R getField(R T::*pmo, Arg &&arg) { return std::forward<Arg>(arg).*pmo; }
template <typename R, typename T, typename Arg, typename... Args> auto callMember(R T::*pmf, Arg &&arg, Args &&... args) -> decltype((std::forward<Arg>(arg).*pmf)(std::forward<Args>(args)...)) {
    return (std::forward<Arg>(arg).*pmf)(std::forward<Args>(args)...);
}
typedef int (P::*Getter)();
struct Timer { P *object; int (P::*function)(); int fire() { return (object->*function)(); } };
class S { public: typedef S *(S::*UnspecifiedBool); operator UnspecifiedBool() const { return ok ? &S::self : nullptr; } S *self; bool ok; };
int main() {
    P p{3, 4, 1.5};
    int P::*field = &P::y;
    double P::*dz = &P::z;
    printf("fields %d %d %g\n", p.*field, (&p)->*field, p.*dz);
    p.*field = 40;
    printf("set %d\n", p.y);
    int (P::*m)(int) const = &P::sum;
    printf("call %d %d\n", (p.*m)(1), ((&p)->*m)(2));
    Getter g = &P::twice;
    printf("typedef %d\n", (p.*g)());
    printf("templates %d %d %d\n", getField(&P::x, p), callMember(&P::sum, p, 10), callMember(&P::twice, p));
    Timer t{&p, &P::twice};
    printf("timer %d\n", t.fire());
    int P::*none = nullptr;
    printf("null %d %d\n", (int)(none == nullptr), (int)(field != nullptr));
    S s; s.ok = true; S s2; s2.ok = false;
    printf("bool %d %d\n", (int)(bool)s, (int)(bool)s2);
    return 0;
}
