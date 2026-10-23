// A condition that declares a reference: the object referred to is the
// condition (a class's conversion to bool, a pointer's null test), not
// the reference itself, which is never null (WebCore's
// SystemFontDatabase::systemFontShorthandInfo: `if (auto& entry =
// cache[shorthand]) return *entry;` of a std::optional).
#include <optional>
#include <stdio.h>

struct Info {
    int size;
};

static std::optional<Info> cache[3];

static const Info& infoOf(int index)
{
    if (auto& entry = cache[index])
        return *entry;
    cache[index] = Info { 16 + index };
    return *cache[index];
}

struct Flag {
    bool on;
    explicit operator bool() const { return on; }
};

int main()
{
    printf("%d %d %d\n", infoOf(1).size, infoOf(1).size, infoOf(2).size);
    Flag off { false };
    Flag on { true };
    if (Flag& flag = off)
        printf("off is true\n");
    else
        printf("off is false\n");
    if (const Flag& flag = on)
        printf("on is true\n");
    int* nothing = nullptr;
    int value = 7;
    int* something = &value;
    if (int*& pointer = nothing)
        printf("null is true\n");
    else
        printf("null is false\n");
    if (int*& pointer = something)
        printf("pointer %d\n", *pointer);
    int countdown = 3;
    int rounds = 0;
    while (int& left = countdown) {
        --left;
        ++rounds;
    }
    printf("rounds %d\n", rounds);
    switch (int& chosen = value) {
    case 7:
        printf("switch 7\n");
        break;
    default:
        printf("switch other\n");
    }
    return 0;
}
