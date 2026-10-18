// __builtin_frame_address(n) and __builtin_return_address(n) walk the frame pointer chain (JSC's DECLARE_CALL_FRAME takes the JS call frame as frame_address(1)).
#include <stdio.h>
#include <stdint.h>

namespace p_frame {
static void *outerFrame;
static void *retSeen;
__attribute__((noinline)) void inner() {
    void *f1 = __builtin_frame_address(1);
    printf("%d\n", f1 == outerFrame);
    retSeen = __builtin_return_address(0);
}
__attribute__((noinline)) void outer() {
    outerFrame = __builtin_frame_address(0);
    inner();
    printf("%d\n", retSeen != 0);
}
int run() {
    char local;
    void *f0 = __builtin_frame_address(0);
    printf("%d\n", f0 != 0 && (char *)f0 > &local && (char *)f0 - &local < 4096);
    outer();
    return 0;
}
}

int main() {
    p_frame::run();
    return 0;
}
