// A member template's instance named (its address given, not called)
// before the template's definition is read is made once it is (JSC's
// JIT: `getCTIStub(generateOpGetFromScopeThunk<GlobalVar>)`).
#include <stdio.h>
enum ResolveType { GlobalVar, ClosureVar };
struct VM { int id; int getCTIStub(int (*generator)(VM&)) { return generator(*this); } VM& vm() { return *this; } };
struct JIT {
    VM& m_vm;
    VM& vm() { return m_vm; }
    int emit(ResolveType type);
private:
    template<ResolveType> static int generateThunk(VM&);
};
int JIT::emit(ResolveType type)
{
    int code = 0;
    switch (type) {
    case GlobalVar: code = vm().getCTIStub(generateThunk<GlobalVar>); break;
    default: code = vm().getCTIStub(generateThunk<ClosureVar>); break;
    }
    return code;
}
template<ResolveType type> int JIT::generateThunk(VM& vm) { return vm.id * 10 + (int)type; }
int main() { VM vm { 4 }; JIT j { vm }; printf("%d %d\n", j.emit(GlobalVar), j.emit(ClosureVar)); return 0; }
