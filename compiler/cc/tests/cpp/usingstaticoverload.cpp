// A class's static member function does not hide the one of another
// parameter type its using-declaration names: `using X86_64::invert;`
// beside `static invert(DoubleCondition)` (JSC's MacroAssembler; the
// DFG's branches were inverted as doubles').
#include <stdio.h>
struct AbstractBase {
    enum StatusCondition { Success, Failure };
    static StatusCondition invert(StatusCondition c) { return c == Success ? Failure : Success; }
};
template<class A> struct Abstract : AbstractBase { };
struct X86Assembler { };
struct X86Common : Abstract<X86Assembler> {
    enum RelationalCondition { Equal = 4, NotEqual = 5, Above = 7, BelowOrEqual = 6 };
    enum ResultCondition { Zero = 4, NonZero = 5 };
    enum DoubleCondition { DoubleEqual = 0x40, DoubleNotEqual = 0x41 };
};
struct X86_64 : X86Common {
    static RelationalCondition invert(RelationalCondition cond) { return static_cast<RelationalCondition>(cond ^ 1); }
};
using MacroAssemblerBase = X86_64;
struct MacroAssembler : MacroAssemblerBase {
    using AbstractBase::invert;
    using MacroAssemblerBase::invert;
    static DoubleCondition invert(DoubleCondition cond) { return cond == DoubleEqual ? DoubleNotEqual : DoubleEqual; }
    static ResultCondition invert(ResultCondition cond) { return cond == Zero ? NonZero : Zero; }
};
struct Helpers : MacroAssembler { };
struct JITCompiler : Helpers { };
struct SpeculativeJIT : JITCompiler {
    int go(JITCompiler::RelationalCondition condition)
    {
        condition = invert(condition);
        return (int)condition * 1000 + (int)invert(DoubleEqual) * 10 + (int)invert(Zero) + (int)invert(Success) * 100000;
    }
};
int main() { SpeculativeJIT s; printf("%d\n", s.go(SpeculativeJIT::Above)); return 0; }
