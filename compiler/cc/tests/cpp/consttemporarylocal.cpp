// A const integer local whose initializer makes a temporary of a class
// with a destructor: the temporary destroyed once, the one made
// (JavaScriptCore's FTL OSR exit compiler: `const size_t
// scratchBufferSize = ... + codeBlock->jitCode()->calleeSaveRegisters()
// ->sizeOfAreaInBytes();` destroyed a RefPtr<JITCode> never made too,
// the initializer having been read once more to see if it is constant).
#include <stdio.h>
static int made = 0, destroyed = 0;
struct Code { int saves = 3; int calleeSaves() const { return saves; } };
struct Ptr {
    Code* p;
    Ptr(Code* c) : p(c) { ++made; }
    Ptr(const Ptr& o) : p(o.p) { ++made; }
    ~Ptr() { ++destroyed; if (p != &global) printf("bad pointer %p\n", (void*)p); }
    Code* operator->() const { return p; }
    static Code global;
};
Code Ptr::global;
struct Block { Ptr jitCode() { return Ptr(&Ptr::global); } };
int main()
{
    Block b;
    Block* block = &b;
    const unsigned long size = sizeof(long) * 4 + block->jitCode()->calleeSaves();
    printf("%lu %d %d\n", size, made, destroyed);
}
