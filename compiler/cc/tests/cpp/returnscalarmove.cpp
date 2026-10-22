// `return local;` of a local of another type than the class result is
// an rvalue (C++20's implicit move), a scalar's too: `return
// referenceBox;` of a CSSBoxType makes WebCore's Style::ShapeOutside
// through ShapeOutside(ShapeBox &&).
#include <cstdio>
enum class BoxType : unsigned char { Missing, Margin, Border };
struct None { };
struct ShapeOutside {
    ShapeOutside(None) : kind(0) { }
    ShapeOutside(BoxType&& box) : kind(10 + static_cast<int>(box)) { }
    ShapeOutside(int&& n) : kind(100 + n) { }
    int kind;
};
ShapeOutside make(int which)
{
    BoxType referenceBox = which > 1 ? BoxType::Border : BoxType::Margin;
    int count = which;
    if (which == 0)
        return None { };
    if (which == 3)
        return count;
    return referenceBox;
}
int main() {
    printf("%d %d %d %d\n", make(0).kind, make(1).kind, make(2).kind, make(3).kind);
}
