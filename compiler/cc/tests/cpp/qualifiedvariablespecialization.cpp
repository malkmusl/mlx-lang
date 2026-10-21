// A variable template specialized by its qualified name at file scope,
// as WebCore's DEFINE_VARIANT_LIKE_CONFORMANCE does it (`template <>
// inline constexpr auto WebCore::TreatAsVariantLike<X> = true;`), and in
// its namespace; concepts read the specializations.
#include <cstdio>

namespace WebCore {
template<typename> inline constexpr auto TreatAsVariantLike = false;
template<typename T> concept VariantLike = TreatAsVariantLike<T>;
namespace CSS {
struct GridAutoFlow { int x; };
struct Other { };
}
}

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::GridAutoFlow> = true;

namespace WebCore {
template<> inline constexpr bool TreatAsVariantLike<int> = true;
}

template<typename T> int kind(const T&) requires WebCore::VariantLike<T> { return 1; }
template<typename T> int kind(const T&) requires (!WebCore::VariantLike<T>) { return 2; }

int main()
{
    std::printf("%d %d %d %d\n", WebCore::TreatAsVariantLike<WebCore::CSS::GridAutoFlow>, WebCore::TreatAsVariantLike<WebCore::CSS::Other>,
        kind(WebCore::CSS::GridAutoFlow { 1 }), kind(WebCore::CSS::Other { }));
    std::printf("%d\n", WebCore::TreatAsVariantLike<int>);
    return 0;
}
