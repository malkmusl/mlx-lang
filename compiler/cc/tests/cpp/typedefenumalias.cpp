// An enumerator named through aliases of an unnamed enum that a typedef
// names in a namespace (JavaScriptCore's GPRInfo.h: `GPRReg::InvalidGPRReg`,
// GPRReg = MacroAssembler::RegisterID = X86Registers::RegisterID, which is
// `typedef enum : int8_t { ..., InvalidGPRReg = -1 } RegisterID;`): the
// enumerators are the namespace's.
#include <stdio.h>

namespace N {
typedef enum : signed char { a, b, Inv = -1 } R;
}

struct S {
    typedef N::R RegisterID;
};

typedef S::RegisterID G;
static constexpr G invalid { G::Inv };
static constexpr N::R second = N::R::b;

int main()
{
    printf("%d %d %d\n", invalid, second, S::RegisterID::a);
    return 0;
}
