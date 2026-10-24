// A virtual function and its const overload of the same parameters are
// two functions with a slot each, overridden each by its own: Skia's
// GrSurface has `virtual GrRenderTarget *asRenderTarget()` and `virtual
// const GrRenderTarget *asRenderTarget() const`, GrRenderTarget overrides
// both inline (the non-const one was taken for the const one's override,
// left out of the vtable and never made).
#include <cstdio>

struct Target;
struct Surface {
    virtual ~Surface() { }
    virtual Target* asTarget() { return nullptr; }
    virtual const Target* asTarget() const { return nullptr; }
    virtual const char* name() & { return "lvalue surface"; }
    virtual const char* name() && { return "rvalue surface"; }
};
struct Target : Surface {
    Target* asTarget() override { std::printf("non-const\n"); return this; }
    const Target* asTarget() const override { std::printf("const\n"); return this; }
    const char* name() & override { return "lvalue target"; }
    const char* name() && override { return "rvalue target"; }
    int value = 7;
};
struct Plain : Surface { };

int main()
{
    Target target;
    Plain plain;
    Surface* surface = &target;
    const Surface* constSurface = &target;
    std::printf("%d\n", surface->asTarget()->value);
    std::printf("%d\n", constSurface->asTarget()->value);
    Surface* other = &plain;
    const Surface* constOther = &plain;
    std::printf("%d %d\n", other->asTarget() == nullptr, constOther->asTarget() == nullptr);
    std::printf("%s\n", surface->name());
    std::printf("%s\n", static_cast<Surface&&>(*surface).name());
    return 0;
}
