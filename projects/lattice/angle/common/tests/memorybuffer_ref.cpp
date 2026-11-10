// The C++ reference of the MemoryBuffer differential test: the same steps
// as memorybuffer_test.mlx, one a line.
#include "common/MemoryBuffer.h"
#include <cstdio>
namespace gl { namespace priv { std::ostream *gSwallowStream = nullptr; } }
using namespace angle;
static void number(unsigned long long v) { printf("%llu ", v); }
static void state(const MemoryBuffer &b) { number(b.size()); number(b.empty() ? 1 : 0); printf("\n"); }
int main() {
    MemoryBuffer b;
    state(b);
    const size_t sizes[5] = {128, 1024, 32, 0, 4096};
    for (size_t size : sizes) { number(b.resize(size) ? 1 : 0); state(b); }
    b.destroy();
    state(b);
    number(b.reserve(256) ? 1 : 0);
    state(b);
    number(b.resize(200) ? 1 : 0);
    state(b);
    b.fill(0xAB);
    number(b.data()[0]);
    number(b.data()[199]);
    printf("\n");
    b.clear();
    state(b);
    number(b.clearAndReserve(1000) ? 1 : 0);
    state(b);
    b.setSizeToCapacity();
    state(b);
    b.setSize(10);
    state(b);
    b.destroy();
    state(b);
    return 0;
}
