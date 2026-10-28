// `using Base::jump;` of a class whose base brought jump() in by its own
// using-declaration beside its jump(Label) (JSC's MacroAssembler and the
// DFG's SpeculativeJIT): every one of them overloads the class's own.
#include <stdio.h>
struct Jump { int v; };
struct Label { int at; };
struct X86 {
    Jump jump() { return Jump { 7 }; }
    void jump(int r, int tag) { }
};
struct MacroAssembler : public X86 {
    using MacroAssemblerBase = X86;
    using MacroAssemblerBase::jump;
    void jump(Label target) { }
};
struct Helpers : public MacroAssembler { };
struct Compiler : public Helpers { };
struct Block { int id; };
struct JIT : public Compiler {
    using Base = Compiler;
    int total = 0;
    void addBranch(const Jump& j, Block* b) { total += j.v * 10 + b->id; }
    using Base::jump;
    void jump(Block* destination, int mode = 0)
    {
        if (mode) return;
        addBranch(jump(), destination);
    }
    int run() { Block b { 3 }; jump(&b); Jump j = jump(); jump(Label { 1 }); return total + j.v * 100; }
};
int main() { JIT j; printf("%d\n", j.run()); return 0; }
