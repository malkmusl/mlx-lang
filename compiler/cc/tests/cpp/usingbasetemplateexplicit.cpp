// `alloc<C>(...)` in a class declaring a member template alloc beside
// `using Base::alloc;`: the base's templates are candidates too (the DFG's
// SpeculativeJIT::emitAllocateJSObjectWithKnownSize).
#include <stdio.h>
struct VM { int id; };
struct Helpers {
    template<typename C, typename S> int alloc(VM& vm, int result, S structure, int size) { return vm.id * 1000 + result * 100 + (int)structure + size + (int)sizeof(C); }
};
struct Base : Helpers { VM m_vm { 7 }; VM& vm() { return m_vm; } };
struct JIT : Base {
    using Base::alloc;
    template<typename C, typename S> int alloc(int result, S structure, int size) { return alloc<C>(vm(), result, structure, size); }
    int run() { return alloc<long>(3, 20, 1); }
};
int main() { JIT j; printf("%d\n", j.run()); return 0; }
