// A compound assignment's operand of a class converting to a scalar
// (`isClobberedByBlock |= clobberedByThisBlock.operand(...)` of JSC's
// DFGArgumentsEliminationPhase: Operands<bool>'s FastBitReference and its
// `operator bool()`): the built-in operator on what it converts to.
#include <stdio.h>
struct BitRef {
    BitRef(unsigned* w, unsigned b) : word(w), bit(b) { }
    operator bool() const { return (*word >> bit) & 1; }
    unsigned* word;
    unsigned bit;
};
struct Bits {
    unsigned w = 5;
    BitRef at(unsigned i) { return BitRef(&w, i); }
};
int main()
{
    Bits bits;
    auto f = [&](unsigned i) {
        bool clobbered = false;
        clobbered |= bits.at(i);
        bool all = true;
        all &= bits.at(i + 1);
        return clobbered * 10 + all;
    };
    printf("%d %d %d\n", f(0), f(1), f(2));
    return 0;
}
