// A static inline function of a C header reached through a template's
// `auto` parameter (Skia's `SkAutoTCallVProc<WebPData, WebPDataClear>`,
// over `SkFunctionObject<P>`, WebPDataClear static inline in libwebp's
// mux_types.h): called by its own (local) symbol, and made though only
// the template's instance calls it, not an undefined reference.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory>
#include <type_traits>
#include <utility>
extern "C" {
typedef struct { const unsigned char *bytes; size_t size; } WebPData;
static inline void WebPDataInit(WebPData *d) { if (d != NULL) memset(d, 0, sizeof(*d)); }
static inline void WebPDataClear(WebPData *d) { if (d != NULL) { free((void *)d->bytes); WebPDataInit(d); printf("cleared\n"); } }
}

template <typename T, T* P> struct OverloadedFunctionObject {
    template <typename... Args>
    auto operator()(Args&&... args) const -> decltype(P(std::forward<Args>(args)...)) {
        return P(std::forward<Args>(args)...);
    }
};
template <auto F> using FunctionObject = OverloadedFunctionObject<std::remove_pointer_t<decltype(F)>, F>;
template <typename T, void (*P)(T*)> class AutoTCallVProc : public std::unique_ptr<T, FunctionObject<P>> {
    using inherited = std::unique_ptr<T, FunctionObject<P>>;
public:
    using inherited::inherited;
    operator T*() const { return this->get(); }
};

int main(int argc, char **argv)
{
    WebPData d;
    WebPDataInit(&d);
    d.bytes = (const unsigned char *)malloc(4);
    {
        AutoTCallVProc<WebPData, WebPDataClear> autoData(&d);
    }
    printf("%zu\n", d.size);
    return argc;
}
