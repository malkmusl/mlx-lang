// An unscoped enum argument promotes to int ([conv.prom]): the (int)
// overload over the (short) and (float) ones (WebCore's
// FontSelectionValue(SkFontStyle::kExtraBlack_Weight) took the raw
// int16_t constructor, so a bold face read weight 400).
#include <cstdio>
#include <cstdint>
struct Value {
    explicit Value(int16_t raw) : raw(raw), how("short") {}
    Value(int v) : raw(v * 4), how("int") {}
    Value(float v) : raw(int(v * 4)), how("float") {}
    int raw;
    const char *how;
};
enum Weight { Thin = 100, Bold = 700, Black = 1000 };
enum Small : unsigned char { Little = 3 };
enum class Scoped : long { Far = 5 };
static const char *f(long) { return "long"; }
static const char *f(int) { return "int"; }
static const char *g(unsigned char) { return "unsigned char"; }
static const char *g(int) { return "int"; }
static const char *g(long) { return "long"; }
static const char *h(short) { return "short"; }
static const char *h(unsigned) { return "unsigned"; }
static const char *h(int) { return "int"; }
int main(int argc, char **)
{
    Value v(Black);
    std::printf("%s %d\n", v.how, v.raw);
    Value w = Value(Bold);
    std::printf("%s %d\n", w.how, w.raw);
    std::printf("%s %s %s\n", f(Thin), g(Little), h(Thin));
    std::printf("%s\n", f(static_cast<long>(Scoped::Far)));
    return argc == 2 ? 0 : 1;
}
