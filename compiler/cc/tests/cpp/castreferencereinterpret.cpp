// A cast of an object to a reference to another (not a class) type is
// that object's bytes as the type ([expr.reinterpret.cast]), written
// through: reinterpret_cast always, `(T &)x` where a static_cast would
// not bind (a reference not const), the way JavaScriptCore's
// MachineContext sets a signal's instruction pointer,
// `reinterpret_cast<void *&>((uintptr_t &) gregs[REG_RIP]) = target;`
// (WebAssembly's fault handler returned to the faulting load for ever).
// `(const long &)i` of an int binds a temporary, as static_cast does.
#include <stdint.h>
#include <stdio.h>

long long registers[4];

static void*& instructionPointer() { return reinterpret_cast<void*&>((uintptr_t&) registers[1]); }

int main()
{
    instructionPointer() = reinterpret_cast<void*>(0x1234);
    (uintptr_t&) registers[2] = 0x56;
    reinterpret_cast<unsigned long&>(registers[3]) = 0x78;
    unsigned value = 0;
    reinterpret_cast<int&>(value) = -1;
    int small = 5;
    const long& widened = (const long&) small;
    small = 6;
    const unsigned& same = reinterpret_cast<const unsigned&>(small);
    small = 7;
    printf("%llx %llx %llx %x %ld %u %p\n", registers[1], registers[2], registers[3], value, widened, same, instructionPointer());
    return 0;
}
