// `CCallHelpers::branchPtr` named in a derived class: MacroAssembler's own
// overloads with those its `using MacroAssemblerBase::branchPtr;` joins,
// not the base's alone (the DFG's JITCompiler::branchLinkableConstant).
#include <stdio.h>
enum Reg { r0, r1 };
struct BaseIndex { Reg base; Reg index; BaseIndex(Reg b, Reg i, int s) : base(b), index(i) { } };
struct ImmPtr { const void* p; explicit ImmPtr(const void* v) : p(v) { } };
struct X86_64 {
    int branchPtr(int c, BaseIndex l, Reg r) { return 5; }
    int branchPtr(int c, BaseIndex l, ImmPtr r) { return 6; }
};
struct MacroAssembler : X86_64 {
    using MacroAssemblerBase = X86_64;
    using MacroAssemblerBase::branchPtr;
    int branchPtr(int c, Reg l, ImmPtr r) { return 2; }
    int branchPtr(int c, Reg l, Reg r) { return 1; }
};
struct AssemblyHelpers : MacroAssembler { };
struct CCallHelpers : AssemblyHelpers { };
struct JITCompiler : CCallHelpers {
    int go(Reg left) { int x = 7; return CCallHelpers::branchPtr(0, left, ImmPtr(&x)) * 100 + CCallHelpers::branchPtr(0, left, r0) * 10 + CCallHelpers::branchPtr(0, BaseIndex(r0, r1, 2), ImmPtr(&x)); }
};
int main() { JITCompiler j; printf("%d\n", j.go(r1)); return 0; }
