// `c ? object : pointer` where the class converts to the pointer and no
// constructor makes the class of a pointer: the class converted
// ([expr.cond]), not a class made of the other side (WebCore's
// SQLiteDatabase: `isNull() ? notOpenErrorMessage : data()`, an
// ASCIILiteral and a const char *, "no matching constructor").
#include <stdio.h>
#include <string.h>
#include <type_traits>

struct Literal {
    template<unsigned long n> constexpr Literal(const char (&text)[n]) : characters(text) {}
    constexpr operator const char *() const { return characters; }
    const char *characters;
};
static constexpr Literal notOpen { "not open" };

struct Number {
    explicit Number(int v) : value(v) {}
    operator int() const { return value * 10; }
    int value;
};

const char *message(bool open, const char *data) { return !open ? notOpen : data; }
const char *message2(bool open, const char *data) { return open ? data : notOpen; }

int main(int argc, char **)
{
    char buffer[] = "opened";
    printf("%s %s %s %s\n", message(false, buffer), message(true, buffer), message2(false, buffer), message2(true, buffer));
    const char *data = buffer;
    static_assert(std::is_same_v<decltype(argc ? notOpen : data), const char *>);
    Number n(4);
    auto a = argc > 5 ? n : 7;
    auto b = argc > 0 ? n : 7;
    static_assert(std::is_same_v<decltype(a), int>);
    printf("%d %d %d\n", a, b, (int)strlen(argc ? notOpen : data));
    return argc;
}
