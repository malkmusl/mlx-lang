// A class template's default argument names its class without making it:
// the traits can be specialized after a member of the class with the
// default (WebCore's Markable<GlyphOverflow> before
// `template<> struct MarkableTraits<GlyphOverflow>`).
#include <stdio.h>

template<class T> struct MarkableTraits {
    static constexpr bool isEmptyValue(const T &value) { return value.isEmpty(); }
    static constexpr T emptyValue() { return T::emptyValue(); }
};

template<class T, class Traits = MarkableTraits<T>> struct Markable {
    constexpr Markable() : value(Traits::emptyValue()) { }
    constexpr Markable(const T &v) : value(v) { }
    explicit operator bool() const { return !Traits::isEmptyValue(value); }
    T value;
};

struct GlyphOverflow { int left; int right; };

struct CacheEntry {
    Markable<GlyphOverflow> glyphOverflow;
    int width;
};

template<> struct MarkableTraits<GlyphOverflow> {
    static constexpr bool isEmptyValue(const GlyphOverflow &value) { return value.left < 0; }
    static constexpr GlyphOverflow emptyValue() { return { -1, -1 }; }
};

int main(int argc, char **argv)
{
    CacheEntry empty { };
    CacheEntry full { GlyphOverflow { 3, 4 }, 7 };
    printf("%d %d %d\n", (int)bool(empty.glyphOverflow), (int)bool(full.glyphOverflow), full.glyphOverflow.value.right + argc);
    return argc - 2;
}
