// WTF/libpas forms: bool bit-fields read as 0 or 1, globals aligned beyond 16 (alignas(16384), the GOT before the data)
#include <stdio.h>
#include <stdint.h>

namespace p_bf2 {
enum Kind { ka, kb, kc };
struct RC { Kind kind : 8; bool stat : 1; bool part : 1; unsigned bound; };
static RC rc = { kb, false, true, 3 };
int check(RC* r, void* parent) { return r->part == !!parent; }
int run() { int d; printf("%d %d %d %d\n", check(&rc, &d), check(&rc, nullptr), (int)rc.part, (int)rc.kind); return 0; }
}

namespace p_al1 {
char before_ = 1;
alignas(16384) uint64_t g_config[2048];
alignas(64) char line[10];
int run() { printf("%d %d %d\n", (int)((uintptr_t)g_config % 16384), (int)((uintptr_t)line % 64), (int)alignof(decltype(g_config))); return 0; }
}

int main() {
    p_bf2::run();
    p_al1::run();
    return 0;
}
