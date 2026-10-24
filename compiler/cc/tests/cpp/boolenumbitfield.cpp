// A bit-field of an enum whose underlying type is bool reads back
// zero-extended, as a bool's does (WebCore's ScriptElement keeps
// `ParserInserted m_parserInserted : 1`, an `enum class : bool`: read as a
// signed bit, Yes was -1, never equal to ParserInserted::Yes, and no
// parser-inserted external script blocked the parser).
#include <stdio.h>

enum class ParserInserted : bool { No, Yes };
enum Plain : bool { Off, On };
enum class Small : unsigned char { A, B, C };

struct Element {
    ParserInserted parserInserted : 1;
    Plain plain : 1;
    Small small : 2;
    bool flag : 1;
    Element(bool inserted)
        : parserInserted(inserted ? ParserInserted::Yes : ParserInserted::No)
        , plain(On)
        , small(Small::C)
        , flag(true)
    {
    }
};

int main()
{
    Element element(true);
    printf("%d %d %d %d\n", element.parserInserted == ParserInserted::Yes, element.plain == On, element.small == Small::C, element.flag);
    element.parserInserted = ParserInserted::No;
    printf("%d\n", element.parserInserted == ParserInserted::No);
    element.parserInserted = ParserInserted::Yes;
    printf("%d %d\n", element.parserInserted == ParserInserted::Yes, static_cast<int>(element.parserInserted));
    return 0;
}
