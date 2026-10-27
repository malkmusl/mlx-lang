// A non-const lvalue reference parameter takes no argument of another
// type (Skia's GrFragmentProcessor: `fFlags &= flags` of a uint32_t is
// the built-in &=, not the enum's friend `operator&=(Flags &, Flags)` on
// a temporary Flags; its flags were never cleared).
#include <cstdio>
#include <cstdint>
#include <type_traits>
#define SK_MAKE_BITFIELD_OPS(X) \
    inline X operator ~(X a) { using U = std::underlying_type_t<X>; return (X) (~static_cast<U>(a)); } \
    inline X operator |(X a, X b) { using U = std::underlying_type_t<X>; return (X) (static_cast<U>(a) | static_cast<U>(b)); } \
    inline X& operator |=(X& a, X b) { return (a = a | b); } \
    inline X operator &(X a, X b) { using U = std::underlying_type_t<X>; return (X) (static_cast<U>(a) & static_cast<U>(b)); } \
    inline X& operator &=(X& a, X b) { return (a = a & b); }
#define SK_DECL_BITFIELD_OPS_FRIENDS(X) \
    friend X operator ~(X a); friend X operator |(X a, X b); friend X& operator |=(X& a, X b); \
    friend X operator &(X a, X b); friend X& operator &=(X& a, X b);
class FP {
public:
    enum { kWillReadDstColor_Flag = 0x8, kIsBlend = 0x10 };
    bool hasConst() const { return fFlags & kConstantOutputForConstantInput_OptimizationFlag; }
    uint32_t raw() const { return fFlags; }
    void merge(const FP* other) { mergeOptimizationFlags(ProcessorOptimizationFlags(other)); }
    void clearConst() { fFlags &= ~kConstantOutputForConstantInput_OptimizationFlag; }
    void setRead() { fFlags |= kWillReadDstColor_Flag; }
protected:
    enum OptimizationFlags : uint32_t {
        kNone_OptimizationFlags,
        kCompatibleWithCoverageAsAlpha_OptimizationFlag = 0x1,
        kPreservesOpaqueInput_OptimizationFlag = 0x2,
        kConstantOutputForConstantInput_OptimizationFlag = 0x4,
        kAll_OptimizationFlags = kCompatibleWithCoverageAsAlpha_OptimizationFlag | kPreservesOpaqueInput_OptimizationFlag | kConstantOutputForConstantInput_OptimizationFlag
    };
    SK_DECL_BITFIELD_OPS_FRIENDS(OptimizationFlags)
public:
    FP(OptimizationFlags f) : fFlags(f) { }
    static FP* leaf() { return new FP(kCompatibleWithCoverageAsAlpha_OptimizationFlag); }
    static FP* all() { return new FP(kAll_OptimizationFlags); }
    OptimizationFlags optimizationFlags() const { return static_cast<OptimizationFlags>(kAll_OptimizationFlags & fFlags); }
    static OptimizationFlags ProcessorOptimizationFlags(const FP* fp) { return fp ? fp->optimizationFlags() : kAll_OptimizationFlags; }
    void mergeOptimizationFlags(OptimizationFlags flags) { fFlags &= (flags | ~kAll_OptimizationFlags); }
private:
    uint32_t fFlags = 0;
};
SK_MAKE_BITFIELD_OPS(FP::OptimizationFlags)
int main()
{
    FP* leaf = FP::leaf();
    FP* matrix = new FP(FP::ProcessorOptimizationFlags(leaf));
    FP* sksl = FP::all();
    sksl->setRead();
    sksl->merge(matrix);
    std::printf("leaf %u matrix %u sksl %u const %d\n", leaf->raw(), matrix->raw(), sksl->raw(), sksl->hasConst());
    sksl->clearConst();
    std::printf("cleared %u\n", sksl->raw());
    return 0;
}
