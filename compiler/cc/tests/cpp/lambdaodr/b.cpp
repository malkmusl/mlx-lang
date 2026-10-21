#include "lambdas.h"

Pointers fromB()
{
    Pointers p;
    p.counter = counter();
    p.throughPointer = counterThroughPointer();
    p.typed = typedCounter<int>();
    p.member = Widget().make();
    p.memberOther = p.member;
    p.mutableOne = mutableCounter();
    p.nested = nestedCounter();
    p.generic = genericCounter();
    p.variable = sharedLambda();
    p.type = closureType();
    return p;
}
