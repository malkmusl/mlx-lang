// A function template's parameter of a class template's instance naming
// only explicit arguments and a default one (WTF's CodePtr<tag> is
// CodePtr<tag, FunctionAttributes::None>): the argument converts to the
// instance, here through a constructor template, and deduces nothing
// (JSC's `jit.callOperation<OperationPtrTag>(function)`).
#include <stdio.h>
#include <stdint.h>
enum class PtrTag : unsigned { NoTag, OpTag };
enum class Attr : unsigned { None, Other };
template<PtrTag tag, Attr attr = Attr::None>
class CodePtr {
public:
    CodePtr() = default;
    template<typename Out, typename... In>
    constexpr CodePtr(Out (*ptr)(In...)) : m_value(reinterpret_cast<void*>(ptr)) { }
    void* taggedPtr() const { return m_value; }
    void* m_value { nullptr };
};
struct Pair { uintptr_t a, b; };
Pair op(void*, const int*) { return { 1, 2 }; }
int op2(int x) { return x; }
struct Masm {
    template<PtrTag tag>
    void callOperation(const CodePtr<tag> operation) { printf("%d\n", operation.taggedPtr() != nullptr); }
};
template<typename F> void gen(Masm& m, F f) { m.callOperation<PtrTag::OpTag>(f); }
int main() { Masm m; m.callOperation<PtrTag::OpTag>(op); m.callOperation<PtrTag::OpTag>(op2); gen(m, op); auto* fp = &op2; m.callOperation<PtrTag::OpTag>(fp); return 0; }
