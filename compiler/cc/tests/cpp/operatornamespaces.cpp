// An operator's templates in more than two namespaces are all weighed:
// inside WebCore::Style's `operator<<` template, `ts << alternative` of
// a WebCore::Constant found Style's (by name) and WTF's (TextStream's)
// but not WebCore's ("no operator<< for these operands",
// StyleKeyword+Logging.h:36).
#include <stdio.h>
#include <type_traits>

namespace WTF {
struct TextStream {
    TextStream& operator<<(const char* s) { printf("%s", s); return *this; }
    TextStream& operator<<(int v) { printf("%d", v); return *this; }
};
template<typename T> struct Box { T v; };
template<typename T> TextStream& operator<<(TextStream& ts, const Box<T>& b) { return ts << b.v; }
}
using WTF::TextStream;
namespace WebCore {
enum CSSValueID : unsigned short { A = 1, B = 111 };
template<CSSValueID C> struct Constant { };
inline const char* nameLiteral(CSSValueID c) { return c == B ? "b" : "a"; }
template<CSSValueID C> TextStream& operator<<(TextStream& ts, const Constant<C>&) { return ts << nameLiteral(C); }
struct Point { int x; };
inline TextStream& operator<<(TextStream& ts, const Point& p) { return ts << p.x; }

namespace Style {
enum class MaskMode { Alpha, Luminance };
template<typename T> concept EnumWithValueRepresentation = std::is_enum_v<T>;
template<typename F> void valueRepresentation(MaskMode m, F&& f) { if (m == MaskMode::Alpha) f(Constant<A>()); else f(Constant<B>()); }
template<EnumWithValueRepresentation T> TextStream& operator<<(TextStream& ts, const T& value)
{
    valueRepresentation(value, [&](const auto& alternative) { ts << alternative; });
    return ts;
}
struct Other { };
TextStream& operator<<(TextStream& ts, const Other&);
}
}

int main(int argc, char **argv)
{
    TextStream ts;
    ts << WebCore::Style::MaskMode::Luminance << " ";
    ts << WebCore::Style::MaskMode::Alpha << "\n";
    ts << WTF::Box<int> { 5 } << "\n";
    return argc;
}
