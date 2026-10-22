#include "lambdas.h"

// Other inline bodies with lambdas first, and the counters in another
// order than b.cpp's.
static int others()
{
    std::function<int(int)> f = [](int x) { return x + 1; };
    return tripled(2) + f(1) + *typedCounter<long>();
}

Pointers fromA()
{
    Pointers p;
    p.variable = sharedLambda();
    p.generic = genericCounter();
    p.nested = nestedCounter();
    p.mutableOne = mutableCounter();
    p.typed = typedCounter<int>();
    Widget w(3);
    p.member = w.make();
    p.memberOther = Widget().make();
    p.throughPointer = counterThroughPointer();
    p.counter = counter();
    p.type = closureType();
    printf("a %d %d\n", others(), w.get());
    return p;
}

int main()
{
    Pointers a = fromA();
    Pointers b = fromB();
    printf("counter %d\n", a.counter == b.counter);
    printf("through a pointer %d\n", a.throughPointer == b.throughPointer);
    printf("template %d\n", a.typed == b.typed);
    printf("nested %d %d\n", a.nested == b.nested, *a.nested);
    printf("generic %d %d\n", a.generic == b.generic, *a.generic);
    printf("mutable %d\n", a.mutableOne == b.mutableOne);
    printf("member %d %d %d\n", a.member == b.member, a.member == a.memberOther, a.member == Gadget().make());
    printf("variable %d\n", a.variable == b.variable);
    printf("template of the closure %d\n", a.type == b.type);
    return 0;
}
