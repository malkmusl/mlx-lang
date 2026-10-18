// WebKit JSC forms: to_underlying, vexing parse, auto x(e), span::first<N>
#include <stdio.h>
#include <utility>
#include <span>

namespace p_misc1 {
enum class St { A, B, C };
struct VarOffset { int v; explicit VarOffset(int x) : v(x) { } };
struct Entry { int a; unsigned b; Entry(VarOffset o, unsigned f) : a(o.v), b(f) { } };
static int sumFirst(std::span<int> span) { auto f = span.first<2>(); return f[0] + f[1]; }
static int make(int k) { return k * 3; }
int run() {
    int offset = 4;
    Entry e(VarOffset(offset), static_cast<unsigned>(offset > 2 ? 7 : 1));
    auto r(make(5));
    int arr[3] = { 1, 2, 3 };
    printf("%d %d %d %u %d\n", std::to_underlying(St::C), r, e.a, e.b, sumFirst(arr));
    return 0;
}
}

int main() {
    p_misc1::run();
    return 0;
}
