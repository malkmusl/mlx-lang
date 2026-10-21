// ::Name:: into an unnamed namespace, from inside another namespace
// (Skia's ::Light::Distant).
#include <stdio.h>
namespace { int unused = 0; }
namespace sk { struct Light { int v; }; }
namespace {
struct Light {
    int v;
    static Light Distant(int c) { return Light{c}; }
};
}
namespace sk {
int f() { return ::Light::Distant(7).v; }
}
int main() { printf("%d\n", sk::f()); return 0; }
