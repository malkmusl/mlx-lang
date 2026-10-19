// A class only declared in a namespace hides a typedef of the name in a
// namespace around it (WebCore::Style's `struct FontFeatureSettings;`
// beside the alias WebCore::FontFeatureSettings, StyleBuilderState.h): a
// member declared with it then is the same as its definition after the
// class is complete.
#include <cstdio>

namespace W {
template<typename T> struct Tagged { T value; };
using Settings = Tagged<int>;


namespace S {
struct Settings;
class Builder {
public:
    void set(Settings&&);
    int last { 0 };
};
}

namespace S {
struct Settings {
    W::Settings platform;
};
}
namespace S {
inline void Builder::set(Settings&& settings)
{
    last = settings.platform.value;
}
}
}

int main()
{
    W::S::Builder builder;
    builder.set(W::S::Settings { { 7 } });
    printf("%d\n", builder.last);
    return 0;
}
