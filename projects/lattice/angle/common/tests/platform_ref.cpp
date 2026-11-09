// The C++ reference of the platform_helpers differential test.
#include "common/platform_helpers.h"
#include <cstdio>
namespace gl { namespace priv { std::ostream *gSwallowStream = nullptr; } }
using namespace angle;
static void flag(bool v) { printf("%d ", v ? 1 : 0); }
int main() {
    flag(IsWindowsXP()); flag(IsWindowsVista()); flag(IsWindows7()); flag(IsWindows8()); flag(IsWindows10()); flag(IsWindows11());
    flag(IsWindowsXPOrLater()); flag(IsWindowsVistaOrLater()); flag(IsWindows7OrLater()); flag(IsWindows8OrLater()); flag(IsWindows10OrLater()); flag(IsWindows11OrLater());
    flag(Is64Bit());
    printf("\n");
    return 0;
}
