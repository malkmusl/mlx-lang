// `U u = {};` of a union whose first member is a structure or a union
// (Vulkan's VkClearValue, `VkClearValue clearValue = {};` in ANGLE's
// Vulkan backend): zero, as a member of a structure too.
#include <stdio.h>

union Color { float f[4]; int i[4]; };
struct DepthStencil { float depth; unsigned stencil; };
union Clear { Color color; DepthStencil depthStencil; };
union Nested { struct { int a; int b; } pair; long both; };
struct Attachment { int index; Clear clear; };

int main() {
    Clear clear = {};
    Clear filled = {{{1, 2, 3, 4}}};
    Nested nested = {};
    Attachment attachment = {};
    Attachment given = {3, {}};
    printf("%d %g %d %d %u %d %d\n", clear.color.i[3], filled.color.f[2], nested.pair.b, attachment.index, attachment.clear.depthStencil.stencil, given.index, given.clear.color.i[0]);
    return 0;
}
