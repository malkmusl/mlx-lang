// Constructors overloaded by function pointer types (Skia's
// AutoCallback), and a static member function as a template argument
// through `template <auto F>` (Skia's SkFunctionObject<SkMask::FreeImage>).
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

struct Stats { int n; };
enum class Result { kSuccess, kFailed };

class AutoCallback {
public:
    using Context = void *;
    using Callback = void (*)(Context);
    using CallbackWithStats = void (*)(Context, const Stats &);
    using ResultCallback = void (*)(Context, Result);
    AutoCallback(Callback proc, Context ctx) : fKind(1), fCtx(ctx) { proc(ctx); }
    AutoCallback(CallbackWithStats proc, Context ctx) : fKind(2), fCtx(ctx) { proc(ctx, Stats{7}); }
    AutoCallback(ResultCallback proc, Context ctx) : fKind(3), fCtx(ctx) { proc(ctx, Result::kFailed); }
    int fKind;
    Context fCtx;
};

static void plain(void *ctx) { printf("plain %d\n", *(int *)ctx); }
static void withStats(void *ctx, const Stats &s) { printf("stats %d %d\n", *(int *)ctx, s.n); }
static void withResult(void *ctx, Result r) { printf("result %d %d\n", *(int *)ctx, (int)r); }

template <typename T, T *P> struct OverloadedFunctionObject {
    template <typename... Args>
    auto operator()(Args &&... args) const -> decltype(P(std::forward<Args>(args)...)) {
        return P(std::forward<Args>(args)...);
    }
};
template <auto F> using FunctionObject = OverloadedFunctionObject<std::remove_pointer_t<decltype(F)>, F>;

struct MaskBuilder {
    static int freed;
    static void FreeImage(void *image) {
        free(image);
        ++freed;
    }
};
int MaskBuilder::freed = 0;

using AutoMaskFreeImage = std::unique_ptr<unsigned char, FunctionObject<MaskBuilder::FreeImage>>;
using AutoFree = std::unique_ptr<void, FunctionObject<free>>;

int main(int argc, char **argv) {
    int value = 5;
    AutoCallback a(plain, &value), b(withStats, &value), c(withResult, &value);
    printf("%d %d %d\n", a.fKind, b.fKind, c.fKind);
    {
        AutoMaskFreeImage image((unsigned char *)malloc(16));
        AutoFree other(malloc(8));
    }
    printf("%d\n", MaskBuilder::freed);
    return argc;
}
