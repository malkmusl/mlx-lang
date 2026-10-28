// `{ .gpr = r, .kind = GPR }`: a designated member of an anonymous union,
// then a member after it (JSC's wasm ShuffleLocation).
#include <stdio.h>
#include <stdint.h>
enum GPR : int8_t { rax, rcx };
enum FPR : int8_t { xmm0, xmm1 };
void f()
{
    struct Location {
        enum class Kind : uint8_t { None, G, F, S };
        using enum Kind;
        static Location none() { return { .stackOffset = 0, .kind = None }; }
        static Location fromGPR(GPR r) { return { .gpr = r, .kind = G }; }
        static Location fromStack(int32_t o) { return { .stackOffset = o, .kind = S }; }
        union {
            GPR gpr;
            FPR fpr;
            int32_t stackOffset;
        };
        Kind kind { None };
    };
    Location a = Location::fromGPR(rcx), b = Location::fromStack(-16), c = Location::none();
    printf("%d %d %d %d %d\n", (int)a.gpr, (int)a.kind, b.stackOffset, (int)b.kind, (int)c.kind);
}
int main() { f(); return 0; }
