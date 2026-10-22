// A default argument naming an enumerator of the base class's nested
// enum (Layout::ElementBox's `EnumSet<BaseTypeFlag> = { ElementBoxFlag }`).
#include <stdio.h>
struct Box {
    enum BaseTypeFlag : unsigned { InlineTextBoxFlag = 1, ElementBoxFlag = 2 };
    struct Flags {
        Flags(BaseTypeFlag flag) : bits(flag) { }
        unsigned bits;
    };
    Box(Flags flags) : flags(flags) { }
    Flags flags;
};
struct ElementBox : Box {
    ElementBox(int width, Flags flags = { ElementBoxFlag }) : Box(flags), width(width) { }
    int width;
};
int main(int argc, char** argv)
{
    ElementBox box(5);
    ElementBox text(6, { Box::InlineTextBoxFlag });
    printf("%d %u %u\n", box.width, box.flags.bits, text.flags.bits);
    return argc - 1;
}
