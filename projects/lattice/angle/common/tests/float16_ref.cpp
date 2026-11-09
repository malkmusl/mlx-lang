// The C++ reference of the Float16ToFloat32 differential test: every
// 16-bit pattern converted, the float's bits one per line.
#include "common/mathutil.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
namespace gl { namespace priv { std::ostream *gSwallowStream = nullptr; } }
int main() {
    for (unsigned h = 0; h < 65536; h++) {
        float f = gl::float16ToFloat32(static_cast<unsigned short>(h));
        uint32_t u; memcpy(&u, &f, 4);
        printf("%u\n", u);
    }
    return 0;
}
